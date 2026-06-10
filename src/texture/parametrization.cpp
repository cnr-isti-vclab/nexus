#include "parametrization.h"
#include "xatlas.h"
#include "texture_cache.h"
#include "rasterizer.h"
#include "pushpull.h"
#include "uniform_grid.h"
#include "../loaders/objexporter.h"

#include "../core/mappedmesh.h"
#include "../core/thread_pool.h"

#include <cmath>
#include <cassert>
#include <vector>
#include <iostream>
#include <unordered_map>

#include <algorithm>
#include <array>
#include <stdexcept>
#include <limits>
#include <cstring>

namespace nx {

namespace {

void ParameterizeCharts(xatlas::Atlas* atlas) {
	(void)atlas;
	// Parameterization is handled internally by xatlas during ComputeCharts.
}

inline uint8_t to_u8(float v) {
	float c = std::clamp(v, 0.0f, 1.0f);
	return static_cast<uint8_t>(c * 255.0f + 0.5f);
}

inline std::vector<uint8_t> to_rgb8(const std::vector<Vector3f>& src) {
	std::vector<uint8_t> dst(src.size() * 3);
	for (size_t i = 0; i < src.size(); ++i) {
		dst[i * 3 + 0] = to_u8(src[i].x);
		dst[i * 3 + 1] = to_u8(src[i].y);
		dst[i * 3 + 2] = to_u8(src[i].z);
	}
	return dst;
}

inline bool has_slot_in_materials(const std::vector<Material>& materials, Material::TextureSlot slot) {
	for (const Material& material : materials) {
		if (material.hasTextureId(slot)) {
			return true;
		}
	}
	return false;
}

inline Vector3f add(const Vector3f& a, const Vector3f& b) {
	return {a.x + b.x, a.y + b.y, a.z + b.z};
}

inline Vector3f sub(const Vector3f& a, const Vector3f& b) {
	return {a.x - b.x, a.y - b.y, a.z - b.z};
}

inline Vector3f mul(const Vector3f& v, float s) {
	return {v.x * s, v.y * s, v.z * s};
}

inline float dot(const Vector3f& a, const Vector3f& b) {
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline Vector3f cross(const Vector3f& a, const Vector3f& b) {
	return {
		a.y * b.z - a.z * b.y,
		a.z * b.x - a.x * b.z,
		a.x * b.y - a.y * b.x
	};
}

inline float length(const Vector3f& v) {
	return std::sqrt(dot(v, v));
}

inline Vector3f normalize(const Vector3f& v) {
	const float len = length(v);
	if(len <= 1e-20f)
		return {0.0f, 0.0f, 1.0f};
	return mul(v, 1.0f / len);
}

inline float clamp01(float value) {
	if(value < 0.0f)
		return 0.0f;
	if(value > 1.0f)
		return 1.0f;
	return value;
}

inline double uv_triangle_area(const Vector2f& a, const Vector2f& b, const Vector2f& c) {
	const double ab_u = static_cast<double>(b.u) - static_cast<double>(a.u);
	const double ab_v = static_cast<double>(b.v) - static_cast<double>(a.v);
	const double ac_u = static_cast<double>(c.u) - static_cast<double>(a.u);
	const double ac_v = static_cast<double>(c.v) - static_cast<double>(a.v);
	return 0.5 * std::abs(ab_u * ac_v - ab_v * ac_u);
}

inline int closest_power_of_two(double value) {
	assert(std::isfinite(value));
	assert(value > 0.0);

	int upper = 1;
	while(static_cast<double>(upper) < value && upper <= (1 << 29))
		upper <<= 1;
	int lower = upper;
	if(static_cast<double>(upper) > value)
		lower = std::max(1, upper >> 1);

	const double dist_lower = std::abs(value - static_cast<double>(lower));
	const double dist_upper = std::abs(static_cast<double>(upper) - value);
	return (dist_upper < dist_lower) ? upper : lower;
}

int estimate_initial_tex_res(const MappedMesh& mesh,
		const std::vector<Material>& materials,
		const TextureCache& texture_cache) {
	assert(mesh.micronodes.size() > 0);
	assert(mesh.triangles.size() > 0);
	assert(mesh.wedges.size() > 0);
	assert(mesh.texcoords.size() > 0);

	double total_textured_uv_area = 0.0;
	for(size_t i = 0; i < mesh.triangles.size(); i++) {
		const Triangle& triangle = mesh.triangles[i];
		const Wedge& w0 = mesh.wedges[triangle.w[0]];
		const Wedge& w1 = mesh.wedges[triangle.w[1]];
		const Wedge& w2 = mesh.wedges[triangle.w[2]];
		assert(w0.t != NONE && w1.t != NONE && w2.t != NONE);
		assert(w0.t < mesh.texcoords.size());
		assert(w1.t < mesh.texcoords.size());
		assert(w2.t < mesh.texcoords.size());

		const Vector2f& uv0 = mesh.texcoords[w0.t];
		const Vector2f& uv1 = mesh.texcoords[w1.t];
		const Vector2f& uv2 = mesh.texcoords[w2.t];
		total_textured_uv_area += uv_triangle_area(uv0, uv1, uv2);
	}

	std::size_t texture_pixels = 0;
	bool found_texture = false;
	for(const Material& material: materials) {
		for(std::size_t i = 0; i < Material::kTextureSlotCount; ++i) {
			auto slot = static_cast<Material::TextureSlot>(i);
			if(!material.hasTextureId(slot))
				continue;
			const Pyramid* pyramid = texture_cache.get(material.texture_ids[i]);
			if(!pyramid)
				continue;
			texture_pixels = static_cast<std::size_t>(pyramid->width) * static_cast<std::size_t>(pyramid->height);
			found_texture = true;
			break;
		}
		if(found_texture)
			break;
	}
	assert(texture_pixels > 0);

	const double used_pixels = total_textured_uv_area * static_cast<double>(texture_pixels);
	const double pixels_per_micronode = used_pixels / static_cast<double>(mesh.micronodes.size());
	const double target_res = std::sqrt(std::max(1.0, pixels_per_micronode));
	const int tex_res = closest_power_of_two(target_res);
	assert(tex_res > 0);
	return tex_res;
}

std::vector<std::pair<Index, Index>> collect_source_clusters_for_projection(const MappedMesh& mesh,
															 const MappedMesh& next_mesh,
															 const MicroNode& destination_micronode,
															 Index destination_micro_id,
															 const std::vector<Material>& materials) {
	(void)destination_micro_id;
	(void)materials;

	std::vector<Index> parent_ids;
	for(Index cluster_id: destination_micronode.cluster_ids) {
		assert(cluster_id < next_mesh.clusters.size());
		Index parent_id = next_mesh.clusters[cluster_id].node;
		assert(parent_id != NONE);
		parent_ids.push_back(parent_id);
	}
	std::sort(parent_ids.begin(), parent_ids.end());
	parent_ids.erase(std::unique(parent_ids.begin(), parent_ids.end()), parent_ids.end());
	assert(parent_ids.size() > 0);

	std::vector<std::pair<Index, Index>> source_clusters;
	for(Index parent_id: parent_ids) {
		assert(parent_id < mesh.micronodes.size());
		const MicroNode& source_micronode = mesh.micronodes[parent_id];
		for(Index source_cluster_id: source_micronode.cluster_ids) {
			assert(source_cluster_id < mesh.clusters.size());
			source_clusters.push_back({parent_id, source_cluster_id});
		}
	}
	assert(source_clusters.size() > 0);

	return source_clusters;
}

bool intersect_ray_triangle(const Vector3f& origin,
							const Vector3f& dir,
							const Vector3f& a,
							const Vector3f& b,
							const Vector3f& c,
							float& t,
							float& bary_u,
							float& bary_v) {
	const Vector3f e1 = sub(b, a);
	const Vector3f e2 = sub(c, a);
	const Vector3f p = cross(dir, e2);
	const float det = dot(e1, p);

	const float inv_det = 1.0f / det;
	const Vector3f s = sub(origin, a);
	bary_u = dot(s, p) * inv_det;
	if(bary_u < 0.0f || bary_u > 1.0f)
		return false;

	const Vector3f q = cross(s, e1);
	bary_v = dot(dir, q) * inv_det;
	if(bary_v < 0.0f || (bary_u + bary_v) > 1.0f)
		return false;

	t = std::fabs(dot(e2, q) * inv_det);
	return true;
}

Vector3f sample_node_texture(const MappedMesh& mesh,
							Index micro_id,
							const Vector2f& uv,
							Material::TextureSlot slot,
							const std::vector<Material::TextureSlot>& active_slots) {
	assert(micro_id < mesh.node_textures.size());
	const NodeTexture& node_texture = mesh.node_textures[micro_id];
	assert(node_texture.width > 0 && node_texture.height > 0);

	Index slot_local_index = NONE;
	for(Index i = 0; i < active_slots.size(); ++i) {
		if(active_slots[i] == slot) {
			slot_local_index = i;
			break;
		}
	}
	assert(slot_local_index != NONE);

	const int x = static_cast<int>(clamp01(uv.u) * static_cast<float>(node_texture.width - 1));
	const int y = static_cast<int>(clamp01(uv.v) * static_cast<float>(node_texture.height - 1));
	const std::size_t slot_stride = static_cast<std::size_t>(node_texture.width) * static_cast<std::size_t>(node_texture.height) * 3;
	const std::size_t slot_offset = static_cast<std::size_t>(slot_local_index) * slot_stride;

	const std::size_t pixel_offset = node_texture.offset +
		slot_offset +
		(static_cast<std::size_t>(y) * static_cast<std::size_t>(node_texture.width) + static_cast<std::size_t>(x)) * 3;
	const uint8_t* pixel = mesh.texels.data() + pixel_offset;

	return {
		pixel[0] / 255.0f,
		pixel[1] / 255.0f,
		pixel[2] / 255.0f
	};
}

bool project_to_parents(const Vector3f& pos,
						const Vector3f& normal,
						const UniformTriangleGrid& grid,
						Index& out_parent,
						Vector2f& out_uv) {
	const Vector3f n = normalize(normal);
	const Vector3f dirs[2] = {n, mul(n, -1.0f)};

	float best_distance = std::numeric_limits<float>::max();
	bool found = false;

	for(const Vector3f& dir: dirs) {
		const Vector3f origin = add(pos, mul(dir, 1e-4f));
		Index hit_parent = NONE;
		Vector2f hit_uv{0, 0};
		float hit_distance = 0.0f;
		if(!grid.project(origin, dir, hit_parent, hit_uv, hit_distance))
			continue;
		if(hit_distance >= best_distance)
			continue;

		best_distance = hit_distance;
		out_parent = hit_parent;
		out_uv = hit_uv;
		found = true;
	}
	return found;
}

std::vector<uint8_t> rasterize_projected(const MappedMesh& prev_mesh,
						const std::vector<std::pair<Index, Index>>& source_clusters,
						NodeMesh& destination,
						const std::vector<Material::TextureSlot>& active_slots,
						int tex_res) {
	TileMap& tilemap = destination.tilemap;

	const int w = tilemap.width;
	const int h = tilemap.height;
	std::vector<uint8_t> raster_mask(static_cast<size_t>(w) * static_cast<size_t>(h), 0);
	UniformTriangleGrid grid(prev_mesh, source_clusters);
	if(grid.empty())
		throw std::runtime_error("Empty grid!");

	for(const Triangle& tri: destination.triangles) {
		const Wedge& dw0 = destination.wedges[tri.w[0]];
		const Wedge& dw1 = destination.wedges[tri.w[1]];
		const Wedge& dw2 = destination.wedges[tri.w[2]];
		assert(dw0.t != NONE && dw1.t != NONE && dw2.t != NONE);

		const Vector3f& p0 = destination.positions[dw0.p];
		const Vector3f& p1 = destination.positions[dw1.p];
		const Vector3f& p2 = destination.positions[dw2.p];
		const Vector3f& n0 = destination.normals[dw0.n];
		const Vector3f& n1 = destination.normals[dw1.n];
		const Vector3f& n2 = destination.normals[dw2.n];
		const Vector2f& uv0 = destination.texcoords[dw0.t];
		const Vector2f& uv1 = destination.texcoords[dw1.t];
		const Vector2f& uv2 = destination.texcoords[dw2.t];

		const float x0 = uv0.u * static_cast<float>(w - 1);
		const float y0 = uv0.v * static_cast<float>(h - 1);
		const float x1 = uv1.u * static_cast<float>(w - 1);
		const float y1 = uv1.v * static_cast<float>(h - 1);
		const float x2 = uv2.u * static_cast<float>(w - 1);
		const float y2 = uv2.v * static_cast<float>(h - 1);

		const float min_fx = std::floor(std::min({x0, x1, x2}));
		const float max_fx = std::ceil(std::max({x0, x1, x2}));
		const float min_fy = std::floor(std::min({y0, y1, y2}));
		const float max_fy = std::ceil(std::max({y0, y1, y2}));

		const int min_x = std::max(0, static_cast<int>(min_fx));
		const int max_x = std::min(w - 1, static_cast<int>(max_fx));
		const int min_y = std::max(0, static_cast<int>(min_fy));
		const int max_y = std::min(h - 1, static_cast<int>(max_fy));

		const float denom = ((y1 - y2) * (x0 - x2) + (x2 - x1) * (y0 - y2));
		if(std::fabs(denom) < 1e-12f)
			continue;

		for(int py = min_y; py <= max_y; ++py) {
			for(int px = min_x; px <= max_x; ++px) {
				const size_t mask_idx = static_cast<size_t>(py) * static_cast<size_t>(w) + static_cast<size_t>(px);
				const float sx = static_cast<float>(px) + 0.5f;
				const float sy = static_cast<float>(py) + 0.5f;

				const float b0 = ((y1 - y2) * (sx - x2) + (x2 - x1) * (sy - y2)) / denom;
				const float b1 = ((y2 - y0) * (sx - x2) + (x0 - x2) * (sy - y2)) / denom;
				const float b2 = 1.0f - b0 - b1;
				if(b0 < -1e-5f || b1 < -1e-5f || b2 < -1e-5f)
					continue;

				const Vector3f pos = add(add(mul(p0, b0), mul(p1, b1)), mul(p2, b2));
				const Vector3f n = normalize(add(add(mul(n0, b0), mul(n1, b1)), mul(n2, b2)));

				Index parent_id = NONE;
				Vector2f src_uv{0.0f, 0.0f};
				if(!project_to_parents(pos, n, grid, parent_id, src_uv))
					continue;
				raster_mask[mask_idx] = 1;

				for(Material::TextureSlot slot: active_slots) {
					const Vector3f color = sample_node_texture(prev_mesh, parent_id, src_uv, slot, active_slots);
					uint8_t* pixel = tilemap.pixel(px, py, slot);
					pixel[0] = static_cast<uint8_t>(clamp01(color.x) * 255.0f + 0.5f);
					pixel[1] = static_cast<uint8_t>(clamp01(color.y) * 255.0f + 0.5f);
					pixel[2] = static_cast<uint8_t>(clamp01(color.z) * 255.0f + 0.5f);
				}
			}
		}
	}
	return raster_mask;
}


void open_temp_reparam_buffers(std::size_t max_entries,
							   MappedArray<Wedge>& temp_wedges,
							   MappedArray<Vector2f>& temp_texcoords) {

	std::string tmp_w_name = MappedFile::makeTempPath(std::filesystem::temp_directory_path().string(), "nxs_tmp_wedges_");
	std::filesystem::path tmp_w_path = std::filesystem::path(tmp_w_name + ".bin");
	if(!temp_wedges.open(tmp_w_path.string(), MappedFile::READ_WRITE, max_entries)) {
		throw std::runtime_error("Could not create temporary wedge file: " + tmp_w_path.string());
	}

	std::string tmp_t_name = MappedFile::makeTempPath(std::filesystem::temp_directory_path().string(), "nxs_tmp_texcoords_");
	std::filesystem::path tmp_t_path = std::filesystem::path(tmp_t_name + ".bin");
	if(!temp_texcoords.open(tmp_t_path.string(), MappedFile::READ_WRITE, max_entries)) {
		throw std::runtime_error("Could not create temporary texcoords file: " + tmp_t_path.string());
	}

	temp_wedges.resize(0);
	temp_texcoords.resize(0);
}

void rewrite_micronode_wedges_and_texcoords(MappedMesh& mesh,
									const MicroNode& micronode,
									const NodeMesh& remapped_mesh,
									MappedArray<Wedge>& temp_wedges,
									MappedArray<Vector2f>& temp_texcoords) {
	const std::size_t wedge_begin = temp_wedges.size();
	const std::size_t tex_begin = temp_texcoords.size();
	temp_wedges.resize(wedge_begin + remapped_mesh.wedges.size());
	temp_texcoords.resize(tex_begin + remapped_mesh.texcoords.size());

	for(std::size_t i = 0; i < remapped_mesh.texcoords.size(); ++i) {
		temp_texcoords[tex_begin + i] = remapped_mesh.texcoords[i];
	}

	for(std::size_t i = 0; i < remapped_mesh.wedges.size(); ++i) {
		Wedge out = remapped_mesh.wedges[i];
		assert(out.p < remapped_mesh.position_remap.size());
		assert(out.t < remapped_mesh.texcoords.size());
		out.p = remapped_mesh.position_remap[out.p];
		out.n = out.p;
		out.t = static_cast<Index>(tex_begin + static_cast<std::size_t>(out.t));
		temp_wedges[wedge_begin + i] = out;
	}

	//we know the order of the triangles in remapped_mesh is consistent with the clusters ordering.
	std::size_t count = 0;
	for(Index cluster_id: micronode.cluster_ids) {
		const Cluster& cluster = mesh.clusters[cluster_id];
		Index start = cluster.triangle_offset;
		Index end = cluster.triangle_offset + cluster.triangle_count;

		for(Index tri_idx = start; tri_idx < end; tri_idx++) {
			Index new_w[3];
			for(int k = 0; k < 3; ++k) {
				Index merged_w = remapped_mesh.triangles[count].w[k];
				new_w[k] = static_cast<Index>(wedge_begin + static_cast<std::size_t>(merged_w));
			}

			Triangle& tri = mesh.triangles[tri_idx];
			tri.w[0] = new_w[0];
			tri.w[1] = new_w[1];
			tri.w[2] = new_w[2];
			count++;
		}
	}

	assert(count == remapped_mesh.triangles.size());
}

void finalize_reparam_buffers(MappedMesh& mesh,
						 MappedArray<Wedge>& temp_wedges,
						 MappedArray<Vector2f>& temp_texcoords) {
	mesh.wedges.close();
	mesh.wedges = std::move(temp_wedges);
	mesh.texcoords.close();
	mesh.texcoords = std::move(temp_texcoords);

	for(size_t i = 0; i < mesh.wedges.size(); i++) {
		Wedge &w = mesh.wedges[i];
		assert(w.p < mesh.positions.size());
		assert(w.t < mesh.texcoords.size());
		assert(w.n < mesh.normals.size());
	}
}

} // namespace

bool create_parametrization(NodeMesh& mesh, const ParametrizationOptions& options) {
	if (mesh.positions.empty() || mesh.triangles.empty()) {
		return false;
	}

	xatlas::Atlas* atlas = xatlas::Create();
	if (!atlas) {
		return false;
	}

	std::vector<Index> indices;
	indices.reserve(mesh.triangles.size() * 3);
	for (const Triangle& tri : mesh.triangles) {
		for (int k = 0; k < 3; ++k) {
			Index wedge_idx = tri.w[k];
			indices.push_back(mesh.wedges[wedge_idx].p);
		}
	}

	xatlas::MeshDecl mesh_decl{};
	mesh_decl.vertexCount = static_cast<uint32_t>(mesh.positions.size());
	mesh_decl.vertexPositionData = mesh.positions.data();
	mesh_decl.vertexPositionStride = sizeof(Vector3f);
	mesh_decl.indexCount = static_cast<uint32_t>(indices.size());
	mesh_decl.indexData = indices.data();
	mesh_decl.indexFormat = xatlas::IndexFormat::UInt32;
	mesh_decl.faceCount = static_cast<uint32_t>(mesh.triangles.size());

	xatlas::AddMeshError add_error = xatlas::AddMesh(atlas, mesh_decl);
	if (add_error != xatlas::AddMeshError::Success) {
		std::cerr << "xatlas AddMesh failed: " << xatlas::StringForEnum(add_error) << std::endl;
		xatlas::Destroy(atlas);
		return false;
	}

	xatlas::ChartOptions chart_options{};
	chart_options.maxChartArea = options.max_chart_area;
	chart_options.maxBoundaryLength = options.max_boundary_length;
	chart_options.normalDeviationWeight = options.normal_deviation_weight;
	chart_options.roundnessWeight = options.roundness_weight;
	chart_options.straightnessWeight = options.straightness_weight;
	chart_options.normalSeamWeight = options.normal_seam_weight;
	chart_options.textureSeamWeight = options.texture_seam_weight;
	chart_options.maxCost = options.max_cost;
	chart_options.maxIterations = options.max_iterations;
	chart_options.useInputMeshUvs = false;
	xatlas::PackOptions pack_options{};
	pack_options.padding = options.padding;
	pack_options.resolution = options.resolution;
	pack_options.texelsPerUnit = options.resolution > 0 ? 0.0f : options.texels_per_unit;
	pack_options.bruteForce = options.use_bruteforce;
	pack_options.blockAlign = options.block_align;
	pack_options.rotateCharts = options.rotate_charts;
	pack_options.rotateChartsToAxis = options.rotate_charts_to_axis;

	xatlas::ComputeCharts(atlas, chart_options);
	ParameterizeCharts(atlas);
	xatlas::PackCharts(atlas, pack_options);

	if (atlas->meshCount == 0 || atlas->meshes == nullptr) {
		xatlas::Destroy(atlas);
		return false;
	}

	xatlas::Mesh& out_mesh = atlas->meshes[0];
	if (out_mesh.indexCount != indices.size()) {
		xatlas::Destroy(atlas);
		return false;
	}

	float inv_width = atlas->width > 0 ? 1.0f / static_cast<float>(atlas->width) : 1.0f;
	float inv_height = atlas->height > 0 ? 1.0f / static_cast<float>(atlas->height) : 1.0f;

	// xatlas returns per-corner UVs. Rebuild wedges per triangle, then unify by position/normal/UV.
	std::vector<Wedge> new_wedges;
	new_wedges.reserve(mesh.triangles.size() * 3);
	std::vector<Vector2f> texcoords;
	texcoords.reserve(mesh.triangles.size() * 3);

	struct WedgeKey {
		Index p;
		Index n;
		int32_t u;
		int32_t v;
		bool operator==(const WedgeKey& other) const {
			return p == other.p && n == other.n && u == other.u && v == other.v;
		}
	};
	struct WedgeKeyHash {
		std::size_t operator()(const WedgeKey& k) const {
			std::size_t h = std::hash<Index>{}(k.p);
			h ^= std::hash<Index>{}(k.n) + 0x9e3779b9 + (h << 6) + (h >> 2);
			h ^= std::hash<int32_t>{}(k.u) + 0x9e3779b9 + (h << 6) + (h >> 2);
			h ^= std::hash<int32_t>{}(k.v) + 0x9e3779b9 + (h << 6) + (h >> 2);
			return h;
		}
	};
	std::unordered_map<WedgeKey, Index, WedgeKeyHash> wedge_map;
	const float quant = 1e6f;

	for (std::size_t i = 0; i < mesh.triangles.size(); ++i) {
		Triangle& tri = mesh.triangles[i];
		std::size_t base = i * 3;
		for (int k = 0; k < 3; ++k) {
			Index wedge_idx = tri.w[k];
			assert(wedge_idx < mesh.wedges.size());

			uint32_t out_index = out_mesh.indexArray[base + k];
			assert(out_index < out_mesh.vertexCount);

			const xatlas::Vertex& v = out_mesh.vertexArray[out_index];
			Vector2f uv{v.uv[0] * inv_width, v.uv[1] * inv_height};

			const Wedge& old_w = mesh.wedges[wedge_idx];
			int32_t qu = static_cast<int32_t>(std::lround(uv.u * quant));
			int32_t qv = static_cast<int32_t>(std::lround(uv.v * quant));
			WedgeKey key{old_w.p, old_w.n, qu, qv};

			auto it = wedge_map.find(key);
			if (it == wedge_map.end()) {
				Index tex_idx = static_cast<Index>(texcoords.size());
				texcoords.push_back(uv);
				Wedge w;
				w.p = old_w.p;
				w.n = old_w.n;
				w.t = tex_idx;
				Index new_idx = static_cast<Index>(new_wedges.size());
				new_wedges.push_back(w);
				wedge_map.emplace(key, new_idx);
				tri.w[k] = new_idx;
			} else {
				tri.w[k] = it->second;
			}
		}
	}

	mesh.texcoords = std::move(texcoords);
	mesh.wedges = std::move(new_wedges);

	xatlas::Destroy(atlas);
	return true;
}

void rasterize_initial(TextureCache& texture_cache, const std::vector<Material>& materials,
					   NodeMesh &source, NodeMesh &destination, int tex_res) {

	const size_t triangle_count = source.triangles.size();

	std::vector<Vector2f> dst_positions; //stores the texcoords of destination for rasterization
	std::vector<Vector2f> src_uvs;
	std::vector<Index> triangle_material_ids;
	dst_positions.reserve(triangle_count * 3);
	src_uvs.reserve(triangle_count * 3);
	triangle_material_ids.reserve(triangle_count);

	for (size_t tri_idx = 0; tri_idx < triangle_count; ++tri_idx) {
		const Triangle& src_tri = source.triangles[tri_idx];
		const Triangle& dst_tri = destination.triangles[tri_idx];

		Index material_id = source.material_ids[tri_idx];

		triangle_material_ids.push_back(material_id);

		for (int k = 0; k < 3; ++k) {
			const Index src_wi = src_tri.w[k];
			const Index dst_wi = dst_tri.w[k];
			assert(src_wi < source.wedges.size() && "source wedge index out of range");
			assert(dst_wi < destination.wedges.size() && "destination wedge index out of range");

			const Index src_ti = source.wedges[src_wi].t;
			const Index dst_ti = destination.wedges[dst_wi].t;
			assert(src_ti < source.texcoords.size() && "source texcoord index out of range");
			assert(dst_ti < destination.texcoords.size() && "destination texcoord index out of range");

			const Vector2f& src_uv = source.texcoords[src_ti];
			const Vector2f& dst_uv = destination.texcoords[dst_ti];
			dst_positions.push_back(Vector2f{dst_uv.u * static_cast<float>(tex_res), dst_uv.v * static_cast<float>(tex_res)});
			src_uvs.push_back(src_uv);
		}
	}
	TileMap &tilemap = destination.tilemap;

	Rasterizer rasterizer(tex_res, tex_res);
	std::vector<uint8_t> raster_mask = rasterizer.rasterizeTriangles(dst_positions,
								  src_uvs,
								  triangle_material_ids,
								  materials,
								  &texture_cache,
								  &tilemap);
	pushPullFillUnwrittenPixels(tex_res, tex_res, tilemap.texels, raster_mask, materials);
}



//This is done after loading the initial mesh IF mesh has textures
//we need to compute a new parametrization for each microcluster, and reproject the texture.
//for each micronode, we collect the clusters
//create a NodeMesh, reparametrize, create new wedges and textures. (those are local per microcluster).
//replace old wedges and textures,  positions and normals are not touched, triangles only updated in wedge index.
//reprojection is easy because the geometry is not touched, so rasterization in texture space is enough
//Having multiple threads is good but we might want to ensure the same order.

void reparametrize_initial_clusters(MappedMesh& mesh, std::vector<Material> &materials) {

	if(!mesh.has_textures)
		return;

	ParametrizationOptions options;
	TextureCache texture_cache(mesh.dir.string());
	texture_cache.initFromMaterials(materials);

	std::vector<Material::TextureSlot> active_slots;
	active_slots.push_back(Material::TextureSlot::BaseColor);
	for (size_t i = 1; i < Material::kTextureSlotCount; ++i) {
		auto slot = static_cast<Material::TextureSlot>(i);
		if (has_slot_in_materials(materials, slot)) {
			active_slots.push_back(slot);
		}
	}

	const int tex_res = estimate_initial_tex_res(mesh, materials, texture_cache);
	options.resolution = static_cast<uint32_t>(tex_res);
	const int components = static_cast<int>(active_slots.size()) * 3;

	mesh.allocate_node_textures_and_texels(tex_res, components);

	MappedArray<Wedge> temp_wedges;
	MappedArray<Vector2f> temp_texcoords;
	std::size_t max_entries = static_cast<std::size_t>(mesh.triangles.size()) * 3;
	open_temp_reparam_buffers(max_entries, temp_wedges, temp_texcoords);

	dp::thread_pool pool;

	mesh.node_textures.resize(mesh.micronodes.size());
	for (Index micro_id = 0; micro_id < mesh.micronodes.size(); micro_id++) {
		pool.enqueue_detach([&mesh, &temp_wedges, &temp_texcoords, &materials, &texture_cache, &active_slots, options, tex_res, components](Index micro_id) {
			MicroNode& micronode = mesh.micronodes[micro_id];
			NodeMesh merged = merge_micronode_clusters(mesh, micronode);
			NodeMesh original = merged;
			merged.tilemap.width = merged.tilemap.height = tex_res;
			for(Material::TextureSlot slot: active_slots)
				merged.tilemap.addSlot(slot);

			// Reparametrize merged mesh in-place.
			create_parametrization(merged, options);

			rasterize_initial(texture_cache, materials, original, merged, tex_res);
			//export_iobj(merged, materials, "merged_" + std::to_string(micro_id) + ".obj");

			{
				std::lock_guard<std::mutex> lock(mesh.lock);

				NodeTexture &node_texture = mesh.node_textures[micro_id];
				uint8_t* dst = mesh.texels.data() + node_texture.offset;
				std::memcpy(dst, merged.tilemap.texels.data(), merged.tilemap.texels.size());

				rewrite_micronode_wedges_and_texcoords(mesh,
					micronode,
					merged,
					temp_wedges,
					temp_texcoords);
			}
		}, micro_id);
	}
	pool.wait_for_tasks();

	finalize_reparam_buffers(mesh, temp_wedges, temp_texcoords);
}

void reparametrize_clusters(MappedMesh& mesh,
	MappedMesh& next_mesh,
	const std::vector<Material::TextureSlot>& active_slots,
	const std::vector<Material>& materials,
	bool halve_tex_res) {
	if(!next_mesh.has_textures)
		return;
	if(active_slots.empty())
		return;

	ParametrizationOptions options;
	const int previous_tex_res = mesh.node_textures[0].width;
	assert(previous_tex_res > 0);
	int tex_res = halve_tex_res ? std::max(1, previous_tex_res / 2) : previous_tex_res;

	std::cout << "Tex res: " << tex_res << std::endl;
	options.resolution = static_cast<uint32_t>(tex_res);
	const int components = static_cast<int>(active_slots.size()) * 3;

	next_mesh.allocate_node_textures_and_texels(tex_res, components);

	MappedArray<Wedge> temp_wedges;
	MappedArray<Vector2f> temp_texcoords;
	std::size_t max_entries = static_cast<std::size_t>(next_mesh.triangles.size()) * 3;
	open_temp_reparam_buffers(max_entries, temp_wedges, temp_texcoords);

	std::mutex write_lock;
	dp::thread_pool pool;

	for(Index micro_id = 0; micro_id < next_mesh.micronodes.size(); micro_id++) {
		pool.enqueue_detach([&mesh, &next_mesh, &temp_wedges, &temp_texcoords, &write_lock, &active_slots, &materials, options, tex_res, components](Index micro_id) {
			MicroNode& destination_micronode = next_mesh.micronodes[micro_id];

			std::vector<std::pair<Index, Index>> source_clusters = collect_source_clusters_for_projection(
				mesh, next_mesh, destination_micronode, micro_id, materials);

			NodeMesh destination = merge_micronode_clusters(next_mesh, destination_micronode);
			destination_micronode.error *= sqrt(2.0f*destination.triangles.size()/float(tex_res*tex_res));
			TileMap &tilemap = destination.tilemap;
			tilemap.width = tex_res;
			tilemap.height = tex_res;
			tilemap.texture_slots.clear();
			for(Material::TextureSlot slot: active_slots)
				tilemap.addSlot(slot);

			create_parametrization(destination, options);

			std::vector<uint8_t> raster_mask = rasterize_projected(mesh, source_clusters, destination, active_slots, tex_res);
			pushPullFillUnwrittenPixels(tex_res, tex_res, destination.tilemap.texels, raster_mask, materials);

			export_iobj(destination, materials, "projected_cluster_" + std::to_string(micro_id) + ".obj");

			{
				std::lock_guard<std::mutex> lock(write_lock);

				NodeTexture& node_texture = next_mesh.node_textures[micro_id];
				assert(next_mesh.texels.size() >= node_texture.offset + destination.tilemap.texels.size());
				uint8_t* dst = next_mesh.texels.data() + node_texture.offset;
				std::memcpy(dst, destination.tilemap.texels.data(), destination.tilemap.texels.size());

				rewrite_micronode_wedges_and_texcoords(next_mesh,
					destination_micronode,
					destination,
					temp_wedges,
					temp_texcoords);

				for(size_t i = 0; i < temp_wedges.size(); i++) {
					Wedge &w = temp_wedges[i];
					assert(w.p < next_mesh.positions.size());
					assert(w.t < temp_texcoords.size());
					assert(w.n < next_mesh.normals.size());
				}
			}
		}, micro_id);
	}
	pool.wait_for_tasks();

	for(size_t i = 0; i < temp_wedges.size(); i++) {
		Wedge &w = temp_wedges[i];
		assert(w.t < temp_texcoords.size());
	}

	finalize_reparam_buffers(next_mesh, temp_wedges, temp_texcoords);

	for(size_t i = 0; i < next_mesh.wedges.size(); i++) {
		Wedge &w = next_mesh.wedges[i];
		assert(w.t < next_mesh.texcoords.size());
	}
}


} // namespace nx
