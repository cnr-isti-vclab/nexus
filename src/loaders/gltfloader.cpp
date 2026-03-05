#include "gltfloader.h"
#include "gltf.h"
#include "../core/mappedmesh.h"

#include <draco/compression/decode.h>

#include <array>
#include <cassert>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace nx {

// ---------------------------------------------------------------------------
// Matrix helpers (column-major 4×4, matching glTF convention)
// ---------------------------------------------------------------------------

using Mat4 = std::array<float, 16>;

static Mat4 mat4_mul(const Mat4& a, const Mat4& b) {
	Mat4 r{};
	for (int col = 0; col < 4; col++) {
		for (int row = 0; row < 4; row++) {
			float sum = 0.0f;
			for (int k = 0; k < 4; k++)
				sum += a[k * 4 + row] * b[col * 4 + k];
			r[col * 4 + row] = sum;
		}
	}
	return r;
}

static Vector3f mat4_point(const Mat4& m, Vector3f p) {
	return {
		m[0] * p.x + m[4] * p.y + m[8]  * p.z + m[12],
		m[1] * p.x + m[5] * p.y + m[9]  * p.z + m[13],
		m[2] * p.x + m[6] * p.y + m[10] * p.z + m[14]
	};
}

static Vector3f mat4_normal(const Mat4& m, Vector3f n) {
	return {
		m[0] * n.x + m[4] * n.y + m[8]  * n.z,
		m[1] * n.x + m[5] * n.y + m[9]  * n.z,
		m[2] * n.x + m[6] * n.y + m[10] * n.z
	};
}

static Mat4 trs_to_mat4(
	const std::array<float, 3>& t,
	const std::array<float, 4>& r,
	const std::array<float, 3>& s)
{
	float qx = r[0], qy = r[1], qz = r[2], qw = r[3];
	float sx = s[0], sy = s[1], sz = s[2];

	float r00 = 1.0f - 2.0f * (qy*qy + qz*qz);
	float r10 = 2.0f * (qx*qy + qw*qz);
	float r20 = 2.0f * (qx*qz - qw*qy);
	float r01 = 2.0f * (qx*qy - qw*qz);
	float r11 = 1.0f - 2.0f * (qx*qx + qz*qz);
	float r21 = 2.0f * (qy*qz + qw*qx);
	float r02 = 2.0f * (qx*qz + qw*qy);
	float r12 = 2.0f * (qy*qz - qw*qx);
	float r22 = 1.0f - 2.0f * (qx*qx + qy*qy);

	Mat4 m{};
	m[0]  = r00 * sx;  m[1]  = r10 * sx;  m[2]  = r20 * sx;  m[3]  = 0.0f;
	m[4]  = r01 * sy;  m[5]  = r11 * sy;  m[6]  = r21 * sy;  m[7]  = 0.0f;
	m[8]  = r02 * sz;  m[9]  = r12 * sz;  m[10] = r22 * sz;  m[11] = 0.0f;
	m[12] = t[0];      m[13] = t[1];      m[14] = t[2];      m[15] = 1.0f;
	return m;
}

static Mat4 node_to_mat4(const fx::gltf::Node& node) {
	if (node.matrix != fx::gltf::defaults::IdentityMatrix)
		return node.matrix;
	return trs_to_mat4(node.translation, node.rotation, node.scale);
}

// ---------------------------------------------------------------------------
// Draco decompression pass
// Decodes any primitive that carries KHR_draco_mesh_compression and patches
// the document's buffers / bufferViews / accessors in-place so the rest of
// the loader can treat them as plain uncompressed data.
// ---------------------------------------------------------------------------

static void decompress_draco(fx::gltf::Document& doc) {
	using namespace fx::gltf;
	using CT = Accessor::ComponentType;

	for (auto& mesh : doc.meshes) {
		for (auto& primitive : mesh.primitives) {
			if (primitive.extensionsAndExtras.is_null())
				continue;

			auto ext_it = primitive.extensionsAndExtras.find("extensions");
			if (ext_it == primitive.extensionsAndExtras.end())
				continue;

			nlohmann::json& extensions = *ext_it;
			if (!extensions.count("KHR_draco_mesh_compression"))
				continue;

			std::cout << "glTF: Draco-compressed primitive, decompressing..." << std::endl;

			auto& dra = extensions["KHR_draco_mesh_compression"];
			int bv_id = dra["bufferView"];

			BufferView& src_bv      = doc.bufferViews[bv_id];
			Buffer&     src_buf     = doc.buffers[src_bv.buffer];
			const char* ptr         = reinterpret_cast<char*>(src_buf.data.data()) + src_bv.byteOffset;
			uint32_t    encoded_len = src_bv.byteLength;

			draco::DecoderBuffer dbuf;
			dbuf.Init(ptr, encoded_len);

			draco::EncodedGeometryType geom_type =
				draco::Decoder::GetEncodedGeometryType(&dbuf).value();
			assert(geom_type == draco::TRIANGULAR_MESH &&
				"glTF Draco: only triangular mesh geometry is supported");

			draco::Decoder decoder;
			std::unique_ptr<draco::Mesh> dmesh =
				decoder.DecodeMeshFromBuffer(&dbuf).value();

			const int32_t nf    = dmesh->num_faces();
			const int32_t nvert = dmesh->num_points();

			// --- Patch index accessor ---
			Buffer face_buf;
			face_buf.byteLength = static_cast<uint32_t>(nf) * 3 * sizeof(uint32_t);
			face_buf.data.resize(face_buf.byteLength);

			uint32_t* face_ptr = reinterpret_cast<uint32_t*>(face_buf.data.data());
			for (int32_t i = 0; i < nf; i++) {
				draco::FaceIndex f(i);
				for (int k = 0; k < 3; k++)
					face_ptr[i * 3 + k] = dmesh->face(f)[k].value();
			}

			BufferView face_bv;
			face_bv.buffer     = static_cast<int32_t>(doc.buffers.size());
			face_bv.byteLength = face_buf.byteLength;
			face_bv.byteOffset = 0;
			face_bv.byteStride = static_cast<uint32_t>(sizeof(uint32_t) * 3);
			face_bv.target     = BufferView::TargetType::ElementArrayBuffer;
			doc.bufferViews.push_back(face_bv);
			doc.buffers.push_back(std::move(face_buf));

			assert(primitive.indices >= 0);
			Accessor& face_acc     = doc.accessors[primitive.indices];
			face_acc.bufferView    = static_cast<int32_t>(doc.bufferViews.size()) - 1;
			face_acc.byteOffset    = 0;
			face_acc.componentType = CT::UnsignedInt;
			assert(face_acc.count == static_cast<uint32_t>(nf * 3));

			// --- Patch vertex attribute accessors ---
			auto& dra_attrs = dra["attributes"];
			for (auto& item : dra_attrs.items()) {
				const std::string& attr_name = item.key();
				int unique_id = item.value();

				const draco::PointAttribute* attr =
					dmesh->GetAttributeByUniqueId(unique_id);
				assert(attr && "glTF Draco: attribute not found in decoded mesh");

				if (!primitive.attributes.count(attr_name))
					throw std::runtime_error(
						"glTF Draco: attribute mismatch: " + attr_name);

				Accessor& v_acc = doc.accessors[primitive.attributes.at(attr_name)];

				int num_components = 0;
				switch (v_acc.type) {
				case Accessor::Type::Scalar: num_components = 1; break;
				case Accessor::Type::Vec2:   num_components = 2; break;
				case Accessor::Type::Vec3:   num_components = 3; break;
				case Accessor::Type::Vec4:   num_components = 4; break;
				default:
					throw std::runtime_error("glTF Draco: unsupported accessor type");
				}

				int component_size = 0;
				switch (v_acc.componentType) {
				case CT::Float:
				case CT::UnsignedInt:   component_size = 4; break;
				case CT::Short:
				case CT::UnsignedShort: component_size = 2; break;
				case CT::Byte:
				case CT::UnsignedByte:  component_size = 1; break;
				default:
					throw std::runtime_error("glTF Draco: unsupported component type");
				}

				assert(num_components == attr->num_components() &&
					"glTF Draco: component count mismatch");

				const int stride = num_components * component_size;
				Buffer v_buf;
				v_buf.byteLength = static_cast<uint32_t>(nvert) * stride;
				v_buf.data.resize(v_buf.byteLength);

				for (int i = 0; i < nvert; i++) {
					draco::AttributeValueIndex mapped =
						attr->mapped_index(draco::PointIndex(i));
					attr->GetValue(mapped, v_buf.data.data() + i * stride);
				}

				BufferView v_bv;
				v_bv.buffer     = static_cast<int32_t>(doc.buffers.size());
				v_bv.byteLength = v_buf.byteLength;
				v_bv.byteOffset = 0;
				v_bv.byteStride = static_cast<uint32_t>(stride);
				v_bv.target     = BufferView::TargetType::ArrayBuffer;
				doc.bufferViews.push_back(v_bv);
				doc.buffers.push_back(std::move(v_buf));

				v_acc.bufferView = static_cast<int32_t>(doc.bufferViews.size()) - 1;
				v_acc.byteOffset = 0;
			}
		}
	}
}

// ---------------------------------------------------------------------------
// Image / material helpers
// ---------------------------------------------------------------------------

static std::string resolve_image(
	const fx::gltf::Document& doc,
	int texture_index,
	const std::string& base_dir)
{
	assert(texture_index >= 0 && texture_index < (int)doc.textures.size());
	const fx::gltf::Texture& tex = doc.textures[texture_index];
	assert(tex.source >= 0 && tex.source < (int)doc.images.size());
	const fx::gltf::Image& image = doc.images[tex.source];

	if (image.IsEmbeddedResource()) {
		std::string ext = (image.uri.find("image/png") != std::string::npos) ? ".png" : ".jpg";
		std::string out_path = base_dir + "/nxscache_img" + std::to_string(tex.source) + ext;
		if (!fs::exists(out_path)) {
			std::vector<uint8_t> data;
			image.MaterializeData(data);
			std::ofstream f(out_path, std::ios::binary);
			assert(f.is_open() && "Failed to write embedded glTF image to cache");
			f.write(reinterpret_cast<const char*>(data.data()), data.size());
		}
		return out_path;
	}

	if (!image.uri.empty())
		return (fs::path(base_dir) / image.uri).lexically_normal().string();

	// Buffer-view embedded (no URI)
	assert(image.bufferView >= 0 && image.bufferView < (int)doc.bufferViews.size());
	const fx::gltf::BufferView& bv  = doc.bufferViews[image.bufferView];
	const fx::gltf::Buffer&     buf = doc.buffers[bv.buffer];
	std::string ext = (image.mimeType == "image/png") ? ".png" : ".jpg";
	std::string out_path = base_dir + "/nxscache_img" + std::to_string(image.bufferView) + ext;
	if (!fs::exists(out_path)) {
		std::ofstream f(out_path, std::ios::binary);
		assert(f.is_open() && "Failed to write buffer-embedded glTF image to cache");
		f.write(reinterpret_cast<const char*>(buf.data.data() + bv.byteOffset), bv.byteLength);
	}
	return out_path;
}

static Material convert_material(
	const fx::gltf::Material& gm,
	const fx::gltf::Document& doc,
	const std::string& base_dir)
{
	Material m;
	m.type = MaterialType::PBR;
	m.name = gm.name;

	const auto& pbr = gm.pbrMetallicRoughness;
	for (int i = 0; i < 4; i++)
		m.base_color[i] = pbr.baseColorFactor[i];
	m.metallic_factor    = pbr.metallicFactor;
	m.roughness_factor   = pbr.roughnessFactor;
	m.double_sided       = gm.doubleSided;
	m.normal_scale       = gm.normalTexture.scale;
	m.occlusion_strength = gm.occlusionTexture.strength;
	for (int i = 0; i < 3; i++)
		m.emissive_factor[i] = gm.emissiveFactor[i];

	if (!pbr.baseColorTexture.empty())
		m.base_color_texture = resolve_image(doc, pbr.baseColorTexture.index, base_dir);
	if (!pbr.metallicRoughnessTexture.empty())
		m.metallic_roughness_texture = resolve_image(doc, pbr.metallicRoughnessTexture.index, base_dir);
	if (!gm.normalTexture.empty())
		m.normal_texture = resolve_image(doc, gm.normalTexture.index, base_dir);
	if (!gm.occlusionTexture.empty())
		m.occlusion_texture = resolve_image(doc, gm.occlusionTexture.index, base_dir);
	if (!gm.emissiveTexture.empty())
		m.emissive_texture = resolve_image(doc, gm.emissiveTexture.index, base_dir);

	return m;
}

// ---------------------------------------------------------------------------
// Accessor read helpers
// ---------------------------------------------------------------------------

static uint32_t read_index(
	const fx::gltf::Document& doc,
	const fx::gltf::Accessor& acc,
	uint32_t i)
{
	assert(acc.bufferView >= 0);
	const fx::gltf::BufferView& bv = doc.bufferViews[acc.bufferView];
	const uint8_t* base = doc.buffers[bv.buffer].data.data()
		+ bv.byteOffset + acc.byteOffset;

	switch (acc.componentType) {
	case fx::gltf::Accessor::ComponentType::UnsignedByte:
		return base[i];
	case fx::gltf::Accessor::ComponentType::UnsignedShort: {
		uint16_t v; std::memcpy(&v, base + i * 2, 2); return v;
	}
	case fx::gltf::Accessor::ComponentType::UnsignedInt: {
		uint32_t v; std::memcpy(&v, base + i * 4, 4); return v;
	}
	default:
		throw std::runtime_error("glTF: unsupported index component type");
	}
}

static Vector3f read_vec3f(
	const fx::gltf::Document& doc,
	const fx::gltf::Accessor& acc,
	uint32_t i)
{
	assert(acc.bufferView >= 0);
	assert(acc.type          == fx::gltf::Accessor::Type::Vec3);
	assert(acc.componentType == fx::gltf::Accessor::ComponentType::Float);
	const fx::gltf::BufferView& bv = doc.bufferViews[acc.bufferView];
	const uint32_t stride = bv.byteStride ? bv.byteStride : 12u;
	const uint8_t* p = doc.buffers[bv.buffer].data.data()
		+ bv.byteOffset + acc.byteOffset + i * stride;
	Vector3f v;
	std::memcpy(&v.x, p,     4);
	std::memcpy(&v.y, p + 4, 4);
	std::memcpy(&v.z, p + 8, 4);
	return v;
}

static Vector2f read_vec2f(
	const fx::gltf::Document& doc,
	const fx::gltf::Accessor& acc,
	uint32_t i)
{
	assert(acc.bufferView >= 0);
	assert(acc.type          == fx::gltf::Accessor::Type::Vec2);
	assert(acc.componentType == fx::gltf::Accessor::ComponentType::Float);
	const fx::gltf::BufferView& bv = doc.bufferViews[acc.bufferView];
	const uint32_t stride = bv.byteStride ? bv.byteStride : 8u;
	const uint8_t* p = doc.buffers[bv.buffer].data.data()
		+ bv.byteOffset + acc.byteOffset + i * stride;
	Vector2f v;
	std::memcpy(&v.u, p,     4);
	std::memcpy(&v.v, p + 4, 4);
	return v;
}

// Read COLOR_0 element i as Rgba8.
// Supports Vec3/Vec4 × Float / UnsignedByte / UnsignedShort (normalized).
static Rgba8 read_color(
	const fx::gltf::Document& doc,
	const fx::gltf::Accessor& acc,
	uint32_t i)
{
	using CT = fx::gltf::Accessor::ComponentType;
	using AT = fx::gltf::Accessor::Type;
	assert(acc.bufferView >= 0);

	const bool is_vec4 = (acc.type == AT::Vec4);

	int component_size = 0;
	switch (acc.componentType) {
	case CT::Float:         component_size = 4; break;
	case CT::UnsignedByte:  component_size = 1; break;
	case CT::UnsignedShort: component_size = 2; break;
	default:
		throw std::runtime_error("glTF: unsupported COLOR_0 component type");
	}
	const int n_comps = is_vec4 ? 4 : 3;

	const fx::gltf::BufferView& bv = doc.bufferViews[acc.bufferView];
	const uint32_t stride = bv.byteStride
		? bv.byteStride
		: static_cast<uint32_t>(n_comps * component_size);

	const uint8_t* p = doc.buffers[bv.buffer].data.data()
		+ bv.byteOffset + acc.byteOffset + i * stride;

	Rgba8 c{255, 255, 255, 255};

	switch (acc.componentType) {
	case CT::Float: {
		const float* f = reinterpret_cast<const float*>(p);
		auto fb = [](float v) -> uint8_t {
			return static_cast<uint8_t>(std::min(std::max(v * 255.0f, 0.0f), 255.0f));
		};
		c.r = fb(f[0]); c.g = fb(f[1]); c.b = fb(f[2]);
		c.a = is_vec4 ? fb(f[3]) : 255;
		break;
	}
	case CT::UnsignedByte:
		c.r = p[0]; c.g = p[1]; c.b = p[2];
		c.a = is_vec4 ? p[3] : 255;
		break;
	case CT::UnsignedShort: {
		// normalized: divide by 65535, map to 0–255 via high byte
		uint16_t r, g, b, a = 65535;
		std::memcpy(&r, p,     2);
		std::memcpy(&g, p + 2, 2);
		std::memcpy(&b, p + 4, 2);
		if (is_vec4) std::memcpy(&a, p + 6, 2);
		c.r = static_cast<uint8_t>(r >> 8);
		c.g = static_cast<uint8_t>(g >> 8);
		c.b = static_cast<uint8_t>(b >> 8);
		c.a = static_cast<uint8_t>(a >> 8);
		break;
	}
	default: break;
	}
	return c;
}

// ---------------------------------------------------------------------------
// Scene-graph traversal
// ---------------------------------------------------------------------------

struct PrimRef {
	const fx::gltf::Primitive* prim;
	Mat4 world_mat;
};

static void collect_node(
	const fx::gltf::Document& doc,
	int node_idx,
	const Mat4& parent_mat,
	std::vector<PrimRef>& out)
{
	assert(node_idx >= 0 && node_idx < (int)doc.nodes.size());
	const fx::gltf::Node& node = doc.nodes[node_idx];
	Mat4 world = mat4_mul(parent_mat, node_to_mat4(node));

	if (node.mesh >= 0) {
		assert(node.mesh < (int)doc.meshes.size());
		for (const auto& prim : doc.meshes[node.mesh].primitives) {
			if (prim.mode != fx::gltf::Primitive::Mode::Triangles)
				throw std::runtime_error("glTF: only Triangles primitive mode is supported");
			if (prim.indices < 0)
				throw std::runtime_error("glTF: non-indexed primitives are not supported");
			if (!prim.attributes.count("POSITION"))
				throw std::runtime_error("glTF: primitive missing POSITION attribute");
			out.push_back({&prim, world});
		}
	}

	for (int child : node.children)
		collect_node(doc, child, world, out);
}

// ---------------------------------------------------------------------------
// GltfLoader
// ---------------------------------------------------------------------------

GltfLoader::GltfLoader(const std::string& filename)
	: gltf_path(filename)
{}

void GltfLoader::load(MappedMesh& mesh, std::vector<Material>& out_materials) {
	fx::gltf::ReadQuotas quotas{};
	quotas.MaxBufferCount      = 1u << 20;
	quotas.MaxBufferByteLength = 0xffffffffu;
	quotas.MaxFileSize         = 0xffffffffu;

	fx::gltf::Document doc;
	if (gltf_path.size() >= 4 &&
		gltf_path.substr(gltf_path.size() - 4) == ".glb")
		doc = fx::gltf::LoadFromBinary(gltf_path, quotas);
	else
		doc = fx::gltf::LoadFromText(gltf_path, quotas);

	const std::string base_dir = fs::path(gltf_path).parent_path().string();

	// Decompress any Draco-encoded primitives (patches doc in-place)
	decompress_draco(doc);

	// ---- Collect all primitives with world-space transforms ----

	Mat4 identity = fx::gltf::defaults::IdentityMatrix;
	std::vector<PrimRef> prim_refs;

	if (doc.scene >= 0 && doc.scene < (int)doc.scenes.size()) {
		for (uint32_t root : doc.scenes[doc.scene].nodes)
			collect_node(doc, (int)root, identity, prim_refs);
	} else {
		for (int i = 0; i < (int)doc.nodes.size(); i++)
			collect_node(doc, i, identity, prim_refs);
	}

	if (prim_refs.empty())
		throw std::runtime_error("glTF: no triangle primitives found in document");

	// Determine available attributes from first primitive
	bool has_normals   = prim_refs[0].prim->attributes.count("NORMAL")     > 0;
	bool has_texcoords = prim_refs[0].prim->attributes.count("TEXCOORD_0") > 0;
	bool has_colors    = prim_refs[0].prim->attributes.count("COLOR_0")    > 0;

	// ---- Count pass ----

	Index total_pos  = 0;
	Index total_tris = 0;
	bool  need_mats  = false;

	for (const auto& ref : prim_refs) {
		const fx::gltf::Accessor& pos_acc =
			doc.accessors[ref.prim->attributes.at("POSITION")];
		total_pos += pos_acc.count;

		const fx::gltf::Accessor& idx_acc = doc.accessors[ref.prim->indices];
		assert(idx_acc.count % 3 == 0);
		total_tris += idx_acc.count / 3;

		if (ref.prim->material >= 0)
			need_mats = true;
	}

	// ---- Allocate MappedMesh arrays ----

	mesh.positions.resize(total_pos);
	if (has_normals)
		mesh.normals.resize(total_pos);
	if (has_texcoords)
		mesh.texcoords.resize(total_pos);
	if (has_colors)
		mesh.colors.resize(total_pos);

	const Index total_wedges = total_tris * 3;
	mesh.wedges.resize(total_wedges);
	mesh.triangles.resize(total_tris);
	if (need_mats)
		mesh.material_ids.resize(total_tris);

	mesh.has_normals  = has_normals;
	mesh.has_textures = has_texcoords;
	mesh.has_colors   = has_colors;

	// ---- Fill pass ----

	Aabb bounds;
	bool first_vertex = true;

	Index pos_base   = 0;
	Index tri_base   = 0;
	Index wedge_base = 0;

	for (const auto& ref : prim_refs) {
		const fx::gltf::Primitive& prim = *ref.prim;
		const Mat4& world = ref.world_mat;

		const fx::gltf::Accessor& pos_acc =
			doc.accessors[prim.attributes.at("POSITION")];
		const uint32_t n_verts = pos_acc.count;

		// --- Positions ---
		for (uint32_t i = 0; i < n_verts; i++) {
			Vector3f p = mat4_point(world, read_vec3f(doc, pos_acc, i));
			mesh.positions[pos_base + i] = p;

			if (first_vertex) {
				bounds.min = bounds.max = p;
				first_vertex = false;
			} else {
				bounds.min.x = std::min(bounds.min.x, p.x);
				bounds.min.y = std::min(bounds.min.y, p.y);
				bounds.min.z = std::min(bounds.min.z, p.z);
				bounds.max.x = std::max(bounds.max.x, p.x);
				bounds.max.y = std::max(bounds.max.y, p.y);
				bounds.max.z = std::max(bounds.max.z, p.z);
			}
		}

		// --- Normals ---
		if (has_normals && prim.attributes.count("NORMAL")) {
			const fx::gltf::Accessor& norm_acc =
				doc.accessors[prim.attributes.at("NORMAL")];
			assert(norm_acc.count == n_verts);
			for (uint32_t i = 0; i < n_verts; i++)
				mesh.normals[pos_base + i] =
					mat4_normal(world, read_vec3f(doc, norm_acc, i));
		}

		// --- Texture coordinates ---
		if (has_texcoords && prim.attributes.count("TEXCOORD_0")) {
			const fx::gltf::Accessor& tc_acc =
				doc.accessors[prim.attributes.at("TEXCOORD_0")];
			assert(tc_acc.count == n_verts);
			assert(tc_acc.componentType == fx::gltf::Accessor::ComponentType::Float);
			for (uint32_t i = 0; i < n_verts; i++)
				mesh.texcoords[pos_base + i] = read_vec2f(doc, tc_acc, i);
		}

		// --- Vertex colors ---
		if (has_colors && prim.attributes.count("COLOR_0")) {
			const fx::gltf::Accessor& color_acc =
				doc.accessors[prim.attributes.at("COLOR_0")];
			assert(color_acc.count == n_verts);
			for (uint32_t i = 0; i < n_verts; i++)
				mesh.colors[pos_base + i] = read_color(doc, color_acc, i);
		}

		// --- Indices → wedges + triangles ---
		const fx::gltf::Accessor& idx_acc = doc.accessors[prim.indices];
		const uint32_t n_tris = idx_acc.count / 3;
		const int32_t  mat_id = prim.material;

		for (uint32_t t = 0; t < n_tris; t++) {
			for (int k = 0; k < 3; k++) {
				const uint32_t vi = read_index(doc, idx_acc, t * 3 + k);
				assert(vi < n_verts);

				Wedge& w = mesh.wedges[wedge_base + t * 3 + k];
				w.p = pos_base + vi;
				w.n = has_normals   ? pos_base + vi : NONE;
				w.t = has_texcoords ? pos_base + vi : NONE;

				mesh.triangles[tri_base + t].w[k] = wedge_base + t * 3 + k;
			}
			if (need_mats)
				mesh.material_ids[tri_base + t] = mat_id;
		}

		pos_base   += n_verts;
		wedge_base += n_tris * 3;
		tri_base   += n_tris;
	}

	mesh.bounds = bounds;

	// ---- Materials ----

	out_materials.reserve(doc.materials.size());
	for (const auto& gm : doc.materials)
		out_materials.push_back(convert_material(gm, doc, base_dir));

	if (out_materials.empty()) {
		Material def;
		def.type = MaterialType::PBR;
		def.name = "default";
		out_materials.push_back(def);
	}
}

} // namespace nx
