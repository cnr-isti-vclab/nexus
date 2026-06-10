#include "mesh_hierarchy.h"
#include "spatial_sort.h"
#include "adjacency.h"
#include "clustering.h"
#include "micro_clustering.h"
#include "json.hpp"
#include <fstream>

#include "../nxsbuild/build_parameters.h"
#include "../nxsbuild/merge_simplify_split.h"
#include "../loaders/objexporter.h"
#include "../loaders/plyexporter.h"
#include "../core/thread_pool.h"
#include "../texture/parametrization.h"
#include <iostream>
#include <algorithm>
#include <filesystem>
#include <set>
#include <mutex>
#include <cmath>
#include <atomic>
#include <random>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <cassert>
#include <unordered_map>
#include <limits>
#include "../core/log.h"

namespace nx {

namespace {

static void save_levels_list(const std::vector<MappedMesh *>& levels) {
	if (levels.empty())
		return;
	using json = nlohmann::json;
	json j;
	j["levels"] = json::array();
	for (size_t i = 0; i < levels.size(); ++i) {
		json e;
		e["level"] = static_cast<int>(i);
		e["dir"] = levels[i] ? levels[i]->dir.string() : std::string();
		j["levels"].push_back(e);
	}
	std::filesystem::path out = levels[0]->dir / "levels.json";
	std::ofstream o(out.string());
	if (o) {
		o << std::setw(4) << j << std::endl;
		nx::log << "Saved levels list to " << out << std::endl;
	} else {
		nx::log << "Warning: could not write levels list to " << out << std::endl;
	}
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

bool has_texture_path(const Material& material, Material::TextureSlot slot) {
	switch (slot) {
	case Material::TextureSlot::BaseColor:
		return !material.base_color_texture.empty();
	case Material::TextureSlot::MetallicRoughness:
		return !material.metallic_roughness_texture.empty();
	case Material::TextureSlot::Specular:
		return !material.specular_texture.empty();
	case Material::TextureSlot::Normal:
		return !material.normal_texture.empty();
	case Material::TextureSlot::Occlusion:
		return !material.occlusion_texture.empty();
	case Material::TextureSlot::Emissive:
		return !material.emissive_texture.empty();
	default:
		assert(false);
		return false;
	}
}

Index remap_index(std::unordered_map<Index, Index>& remap,
					MappedMesh& dst_mesh,
					const MappedMesh& src_mesh,
					Index src_index,
					bool is_position,
					bool is_normal,
					bool is_texcoord) {
	auto it = remap.find(src_index);
	if (it != remap.end())
		return it->second;

	Index dst_index = NONE;
	if (is_position) {
		dst_index = static_cast<Index>(dst_mesh.positions.size());
		dst_mesh.positions.resize(dst_mesh.positions.size() + 1);
		dst_mesh.positions[dst_index] = src_mesh.positions[src_index];
	}
	if (is_normal) {
		dst_index = static_cast<Index>(dst_mesh.normals.size());
		dst_mesh.normals.resize(dst_mesh.normals.size() + 1);
		dst_mesh.normals[dst_index] = src_mesh.normals[src_index];
	}
	if (is_texcoord) {
		dst_index = static_cast<Index>(dst_mesh.texcoords.size());
		dst_mesh.texcoords.resize(dst_mesh.texcoords.size() + 1);
		dst_mesh.texcoords[dst_index] = src_mesh.texcoords[src_index];
	}
	remap[src_index] = dst_index;
	return dst_index;
}

void export_source_clusters_debug_mesh(const MappedMesh& mesh,
								const std::vector<std::pair<Index, Index>>& source_clusters,
								const std::vector<Material>& materials,
								const std::filesystem::path& output_path) {
	MappedMesh debug_mesh;
	debug_mesh.has_normals = mesh.normals.size() > 0;
	debug_mesh.has_textures = mesh.texcoords.size() > 0;

	std::unordered_map<Index, Index> pos_remap;
	std::unordered_map<Index, Index> nor_remap;
	std::unordered_map<Index, Index> tex_remap;
	std::unordered_map<Index, Index> wedge_remap;

	for(const auto& ref: source_clusters) {
		const Index cluster_id = ref.second;
		assert(cluster_id < mesh.clusters.size());
		const Cluster& cluster = mesh.clusters[cluster_id];
		const Index tri_begin = cluster.triangle_offset;
		const Index tri_end = cluster.triangle_offset + cluster.triangle_count;

		for(Index tri_idx = tri_begin; tri_idx < tri_end; ++tri_idx) {
			assert(tri_idx < mesh.triangles.size());
			Triangle tri_out{};
			const Triangle& tri = mesh.triangles[tri_idx];
			for(int k = 0; k < 3; ++k) {
				Index src_wedge_index = tri.w[k];
				auto wedge_it = wedge_remap.find(src_wedge_index);
				if (wedge_it != wedge_remap.end()) {
					tri_out.w[k] = wedge_it->second;
					continue;
				}

				const Wedge& src_wedge = mesh.wedges[src_wedge_index];
				Wedge dst_wedge{};
				dst_wedge.p = remap_index(pos_remap, debug_mesh, mesh, src_wedge.p, true, false, false);
				if (debug_mesh.has_normals && src_wedge.n != NONE)
					dst_wedge.n = remap_index(nor_remap, debug_mesh, mesh, src_wedge.n, false, true, false);
				else
					dst_wedge.n = NONE;

				if (debug_mesh.has_textures && src_wedge.t != NONE)
					dst_wedge.t = remap_index(tex_remap, debug_mesh, mesh, src_wedge.t, false, false, true);
				else
					dst_wedge.t = NONE;

				Index dst_wedge_index = static_cast<Index>(debug_mesh.wedges.size());
				debug_mesh.wedges.resize(debug_mesh.wedges.size() + 1);
				debug_mesh.wedges[dst_wedge_index] = dst_wedge;
				wedge_remap[src_wedge_index] = dst_wedge_index;
				tri_out.w[k] = dst_wedge_index;
			}

			Index tri_out_index = static_cast<Index>(debug_mesh.triangles.size());
			debug_mesh.triangles.resize(debug_mesh.triangles.size() + 1);
			debug_mesh.triangles[tri_out_index] = tri_out;
			if(tri_idx < mesh.material_ids.size()) {
				debug_mesh.material_ids.resize(debug_mesh.material_ids.size() + 1);
				debug_mesh.material_ids[debug_mesh.material_ids.size() - 1] = mesh.material_ids[tri_idx];
			}
		}
	}

	export_obj(debug_mesh, materials, output_path);
}

std::vector<std::pair<Index, Index>> collect_source_clusters_for_projection(const MappedMesh& mesh,
														 const MappedMesh& next_mesh,
														 const MicroNode& destination_micronode,
														 Index destination_micro_id,
														 const std::vector<Material>& materials) {
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

	t = fabs(dot(e2, q) * inv_det);
	return true;
}

Vector3f triangle_normal(const NodeMesh& mesh, const Triangle& tri) {
	const Wedge& w0 = mesh.wedges[tri.w[0]];
	const Wedge& w1 = mesh.wedges[tri.w[1]];
	const Wedge& w2 = mesh.wedges[tri.w[2]];
	const Vector3f& p0 = mesh.positions[w0.p];
	const Vector3f& p1 = mesh.positions[w1.p];
	const Vector3f& p2 = mesh.positions[w2.p];
	return normalize(cross(sub(p1, p0), sub(p2, p0)));
}

void validate_normals_for_reprojection(const NodeMesh& mesh, Index micro_id) {
	if(mesh.triangles.size() == 0)
		return;
	if(mesh.normals.size() == 0)
		return;

	size_t checked = 0;
	size_t opposite = 0;
	for(const Triangle& tri: mesh.triangles) {
		const Wedge& w0 = mesh.wedges[tri.w[0]];
		const Wedge& w1 = mesh.wedges[tri.w[1]];
		const Wedge& w2 = mesh.wedges[tri.w[2]];
		if(w0.n == NONE || w1.n == NONE || w2.n == NONE)
			continue;
		assert(w0.n < mesh.normals.size());
		assert(w1.n < mesh.normals.size());
		assert(w2.n < mesh.normals.size());

		const Vector3f fn = triangle_normal(mesh, tri);
		const Vector3f vn = normalize(add(add(mesh.normals[w0.n], mesh.normals[w1.n]), mesh.normals[w2.n]));
		float f = dot(fn, vn);
		if(f < 0.0f)
			opposite++;
		checked++;
	}

	nx::log << "Reprojection normal check micro " << micro_id << ": checked=" << checked << " opposite=" << opposite << std::endl;
	assert(checked == 0 || opposite == 0);
}

void validate_intersection_for_reprojection(const NodeMesh& mesh, Index micro_id) {
	if(mesh.triangles.size() == 0)
		return;

	const float eps = 1e-4f;
	size_t checked = 0;
	size_t missed = 0;
	for(const Triangle& tri: mesh.triangles) {
		const Wedge& w0 = mesh.wedges[tri.w[0]];
		const Wedge& w1 = mesh.wedges[tri.w[1]];
		const Wedge& w2 = mesh.wedges[tri.w[2]];
		const Vector3f& p0 = mesh.positions[w0.p];
		const Vector3f& p1 = mesh.positions[w1.p];
		const Vector3f& p2 = mesh.positions[w2.p];
		const Vector3f fn = triangle_normal(mesh, tri);
		const Vector3f centroid = mul(add(add(p0, p1), p2), 1.0f / 3.0f);
		const Vector3f origin = add(centroid, mul(fn, eps));

		float t = 0.0f;
		float u = 0.0f;
		float v = 0.0f;
		if(!intersect_ray_triangle(origin, mul(fn, -1.0f), p0, p1, p2, t, u, v))
			missed++;
		checked++;
	}

	nx::log << "Reprojection intersection check micro " << micro_id << ": checked=" << checked << " missed=" << missed << std::endl;
	assert(checked == 0 || missed == 0);
}

void validate_normals_for_mapped_mesh(const MappedMesh& mesh, const std::string& tag) {
	if(mesh.triangles.size() == 0)
		return;
	if(mesh.normals.size() == 0)
		return;

	size_t checked = 0;
	size_t opposite = 0;
	for(size_t i = 0; i < mesh.triangles.size(); i++) {
		const Triangle& tri =  mesh.triangles[i];
		const Wedge& w0 = mesh.wedges[tri.w[0]];
		const Wedge& w1 = mesh.wedges[tri.w[1]];
		const Wedge& w2 = mesh.wedges[tri.w[2]];
		if(w0.n == NONE || w1.n == NONE || w2.n == NONE)
			continue;
		assert(w0.p < mesh.positions.size());
		assert(w1.p < mesh.positions.size());
		assert(w2.p < mesh.positions.size());
		assert(w0.n < mesh.normals.size());
		assert(w1.n < mesh.normals.size());
		assert(w2.n < mesh.normals.size());

		const Vector3f& p0 = mesh.positions[w0.p];
		const Vector3f& p1 = mesh.positions[w1.p];
		const Vector3f& p2 = mesh.positions[w2.p];
		const Vector3f fn = normalize(cross(sub(p1, p0), sub(p2, p0)));
		const Vector3f vn = normalize(add(add(mesh.normals[w0.n], mesh.normals[w1.n]), mesh.normals[w2.n]));
		if(dot(fn, vn) < 0.0f)
			opposite++;
		checked++;
	}

	nx::log << "Mapped normals check " << tag << ": checked=" << checked << " opposite=" << opposite << std::endl;
	//assert(checked == 0 || opposite == 0);
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

	//TODO bilinear interpolation
	//TODO we need a per mesh data structure that keeps the organization o fhe texels (n of tex and relative components)
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
						const MappedMesh& source_mesh,
						const std::vector<std::pair<Index, Index>>& source_clusters,
						Index& out_parent,
						Vector2f& out_uv) {
	const Vector3f n = normalize(normal);
	const Vector3f dirs[2] = {n, mul(n, -1.0f)};

	float best_distance = std::numeric_limits<float>::max();
	bool found = false;

	for(const auto& ref: source_clusters) {
		const Index parent_id = ref.first;
		const Index cluster_id = ref.second;

		assert(cluster_id < source_mesh.clusters.size());
		const Cluster& cluster = source_mesh.clusters[cluster_id];
		const Index tri_begin = cluster.triangle_offset;
		const Index tri_end = cluster.triangle_offset + cluster.triangle_count;

		for(Index tri_idx = tri_begin; tri_idx < tri_end; ++tri_idx) {
			assert(tri_idx < source_mesh.triangles.size());
			const Triangle& tri = source_mesh.triangles[tri_idx];
			const Wedge& w0 = source_mesh.wedges[tri.w[0]];
			const Wedge& w1 = source_mesh.wedges[tri.w[1]];
			const Wedge& w2 = source_mesh.wedges[tri.w[2]];
			const Vector3f& a = source_mesh.positions[w0.p];
			const Vector3f& b = source_mesh.positions[w1.p];
			const Vector3f& c = source_mesh.positions[w2.p];

			for(const Vector3f& dir: dirs) {
				float t = 0.0f;
				float u = 0.0f;
				float v = 0.0f;
				const Vector3f origin = add(pos, mul(dir, 1e-4f));
				if(!intersect_ray_triangle(origin, dir, a, b, c, t, u, v))
					continue;

				if(t >= best_distance)
					continue;

				const float w = 1.0f - u - v;
				assert(w0.t != NONE && w1.t != NONE && w2.t != NONE);
				assert(w0.t < source_mesh.texcoords.size());
				assert(w1.t < source_mesh.texcoords.size());
				assert(w2.t < source_mesh.texcoords.size());
				const Vector2f& uv0 = source_mesh.texcoords[w0.t];
				const Vector2f& uv1 = source_mesh.texcoords[w1.t];
				const Vector2f& uv2 = source_mesh.texcoords[w2.t];

				out_uv.u = uv0.u * w + uv1.u * u + uv2.u * v;
				out_uv.v = uv0.v * w + uv1.v * u + uv2.v * v;
				out_parent = parent_id;
				best_distance = t;
				found = true;
			}
		}
	}
	return found;
}

void rasterize_projected(const MappedMesh& prev_mesh,
						const std::vector<std::pair<Index, Index>>& source_clusters,
						NodeMesh& destination,
						const std::vector<Material::TextureSlot>& active_slots,
						int tex_res) {
	TileMap& tilemap = destination.tilemap;
	tilemap.width = tex_res;
	tilemap.height = tex_res;
	tilemap.texture_slots.clear();
	for(Material::TextureSlot slot: active_slots)
		tilemap.addSlot(slot);

	const int w = tex_res;
	const int h = tex_res;

	for(const Triangle& tri: destination.triangles) {
		const Wedge& dw0 = destination.wedges[tri.w[0]];
		const Wedge& dw1 = destination.wedges[tri.w[1]];
		const Wedge& dw2 = destination.wedges[tri.w[2]];
		assert(dw0.t != NONE && dw1.t != NONE && dw2.t != NONE);
//		assert(dw0.n != NONE && dw1.n != NONE && dw2.n != NONE);

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
				const float sx = static_cast<float>(px) + 0.5f;
				const float sy = static_cast<float>(py) + 0.5f;

				const float b0 = ((y1 - y2) * (sx - x2) + (x2 - x1) * (sy - y2)) / denom;
				const float b1 = ((y2 - y0) * (sx - x2) + (x0 - x2) * (sy - y2)) / denom;
				const float b2 = 1.0f - b0 - b1;
				if(b0 < -1e-5f || b1 < -1e-5f || b2 < -1e-5f)
					continue;

				const Vector3f pos = add(add(mul(p0, b0), mul(p1, b1)), mul(p2, b2));
				const Vector3f n = normalize(add(add(mul(n0, b0), mul(n1, b1)), mul(n2, b2)));

				for(Material::TextureSlot slot: active_slots) {
					uint8_t* pixel = tilemap.pixel(px, py, slot);
					pixel[0] = 0;
					pixel[1] = 0;
					pixel[2] = 255;
				}

				Index parent_id = NONE;
				Vector2f src_uv{0.0f, 0.0f};
				if(!project_to_parents(pos, n, prev_mesh, source_clusters, parent_id, src_uv))
					continue;


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
}

} // namespace



void copyVertices(MappedMesh &source, MappedMesh &target) {
	target.positions.resize(source.positions.size());
	std::copy(source.positions.data(), source.positions.data() + source.positions.size(),
			  target.positions.data());
	if (source.colors.size() > 0) {
		target.colors.resize(source.colors.size());
		std::copy(source.colors.data(), source.colors.data() + source.colors.size(),
				  target.colors.data());
	}

	if (source.material_ids.size() > 0) {
		target.material_ids.resize(source.material_ids.size());
		std::copy(source.material_ids.data(), source.material_ids.data() + source.material_ids.size(),
				  target.material_ids.data());
	}
}

void compactPositions(MappedMesh& mesh) {
	// Compact positions based on wedge usage
	std::size_t pos_count = mesh.positions.size();
	std::vector<uint8_t> pos_used(pos_count, 0);
	for(int i = 0; i < mesh.wedges.size(); i++) {
		const Wedge &w = mesh.wedges[i];
		pos_used[w.p] = 1;
	}

	std::vector<Index> pos_remap(pos_count, NONE);
	Index new_pos_count = 0;
	for (Index i = 0; i < pos_count; ++i) {
		if (pos_used[i]) {
			pos_remap[i] = new_pos_count;
			if (new_pos_count != i) {
				mesh.positions[new_pos_count] = mesh.positions[i];
				if (mesh.has_colors) {
					mesh.colors[new_pos_count] = mesh.colors[i];
				}
			}
			new_pos_count++;
		}
	}
	mesh.positions.resize(new_pos_count);
	if (mesh.has_colors) {
		mesh.colors.resize(new_pos_count);
	}

	for(int i = 0; i < mesh.wedges.size(); i++) {
		Wedge &w = mesh.wedges[i];
		w.p = pos_remap[w.p];
	}
}

void compactNormals(MappedMesh& mesh) {
	if(!mesh.has_normals || mesh.normals.size() == 0)
		return;
	std::size_t n_count = mesh.normals.size();
	std::vector<uint8_t> pos_used(n_count, 0);
	for(int i = 0; i < mesh.wedges.size(); i++) {
		const Wedge &w = mesh.wedges[i];
		assert(w.n != NONE);
		pos_used[w.n] = 1;
	}

	std::vector<Index> pos_remap(n_count, NONE);
	Index new_pos_count = 0;
	for (Index i = 0; i < n_count; ++i) {
		if (pos_used[i]) {
			pos_remap[i] = new_pos_count;
			if (new_pos_count != i) {
				mesh.normals[new_pos_count] = mesh.normals[i];
			}
			new_pos_count++;
		}
	}
	mesh.normals.resize(new_pos_count);
	for(int i = 0; i < mesh.wedges.size(); i++) {
		Wedge &w = mesh.wedges[i];
		w.n = pos_remap[w.n];
	}
}

void compactTextures(MappedMesh& mesh) {
	if(!mesh.has_textures || mesh.texcoords.size() == 0)
		return;

	std::size_t tex_count = mesh.texcoords.size();
	std::vector<uint8_t> pos_used(tex_count, 0);
	for(int i = 0; i < mesh.wedges.size(); i++) {
		const Wedge &w = mesh.wedges[i];
		pos_used[w.t] = 1;
	}

	std::vector<Index> tex_remap(tex_count, NONE);
	Index new_tex_count = 0;
	for (Index i = 0; i < tex_count; ++i) {
		if (pos_used[i]) {
			tex_remap[i] = new_tex_count;
			if (new_tex_count != i) {
				mesh.texcoords[new_tex_count] = mesh.texcoords[i];
			}
			new_tex_count++;
		}
	}
	mesh.texcoords.resize(new_tex_count);

	for(int i = 0; i < mesh.wedges.size(); i++) {
		Wedge &w = mesh.wedges[i];
		w.t = tex_remap[w.t];
	}
}

void compact_mesh(MappedMesh& mesh) {
	// Compact wedges based on triangle usage
	std::size_t wedge_count = mesh.wedges.size();
	std::vector<uint8_t> wedge_used(wedge_count, 0);
	for(size_t i = 0; i < mesh.triangles.size(); i++) {
		Triangle &tri = mesh.triangles[i];
		for (int k = 0; k < 3; ++k) {
			Index w = tri.w[k];
			if (w < wedge_count) {
				wedge_used[w] = 1;
			}
		}
	}

	std::vector<Index> wedge_remap(wedge_count, NONE);
	Index new_wedge_count = 0;
	for (Index i = 0; i < wedge_count; ++i) {
		if (wedge_used[i]) {
			wedge_remap[i] = new_wedge_count;
			if (new_wedge_count != i) {
				mesh.wedges[new_wedge_count] = mesh.wedges[i];
			}
			new_wedge_count++;
		}
	}
	if(mesh.wedges.size() == new_wedge_count)
		std::cout << "Nothing tocompact" << std::endl;
	else
		std::cout << "Compacted> " << std:: endl;
	mesh.wedges.resize(new_wedge_count);

	for(size_t i = 0; i < mesh.triangles.size(); i++) {
		Triangle &tri = mesh.triangles[i];
		for (int k = 0; k < 3; ++k) {
			Index old_w = tri.w[k];
			tri.w[k] = wedge_remap[old_w];
		}
	}

	compactPositions(mesh);
	compactNormals(mesh);
	compactTextures(mesh);
}



//TODO this should identify creases, compute normals and reunify wedges.
void recompute_normals(MappedMesh& mesh) {
	mesh.normals.resize(mesh.positions.size());
	for(size_t i = 0; i < mesh.normals.size(); i++)
		mesh.normals[i] = {0.0f, 0.0f, 0.0f};


	for (size_t i = 0; i < mesh.triangles.size(); ++i) {
		const Triangle &tri = mesh.triangles[i];
		Index w0 = tri.w[0];
		Index w1 = tri.w[1];
		Index w2 = tri.w[2];


		Index v0 = mesh.wedges[w0].p;
		Index v1 = mesh.wedges[w1].p;
		Index v2 = mesh.wedges[w2].p;

		const Vector3f &p0 = mesh.positions[v0];
		const Vector3f &p1 = mesh.positions[v1];
		const Vector3f &p2 = mesh.positions[v2];

		float ux = p1.x - p0.x;
		float uy = p1.y - p0.y;
		float uz = p1.z - p0.z;
		float vx = p2.x - p0.x;
		float vy = p2.y - p0.y;
		float vz = p2.z - p0.z;

		Vector3f n;
		n.x = uy * vz - uz * vy;
		n.y = uz * vx - ux * vz;
		n.z = ux * vy - uy * vx;
;

		Vector3f &n0 = mesh.normals[v0];
		Vector3f &n1 = mesh.normals[v1];
		Vector3f &n2 = mesh.normals[v2];

		n0.x += n.x; n0.y += n.y; n0.z += n.z;
		n1.x += n.x; n1.y += n.y; n1.z += n.z;
		n2.x += n.x; n2.y += n.y; n2.z += n.z;
	}

	for(size_t i = 0; i < mesh.normals.size(); i++) {
		Vector3f &n = mesh.normals[i];
		float len = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
		if (len > 0.0f) {
			n.x /= len;
			n.y /= len;
			n.z /= len;
		} else {
			n.x = n.y = n.z = 0.0f;
		}
	}
	//TODO: if we have creases this will be a bit more complex.
	for(size_t i = 0; i < mesh.wedges.size(); i++) {
		Wedge &w = mesh.wedges[i];
		w.n = w.p;
	}
}

MeshHierarchy::~MeshHierarchy() {
	for(MappedMesh *mesh: levels)
		delete mesh;
	levels.clear();
}

void MeshHierarchy::initialize(MappedMesh *base_mesh, std::vector<Material> &_materials) {
	materials = _materials;
	texture_slots.clear();
	texture_slots.push_back(Material::TextureSlot::BaseColor);
	for(size_t i = 1; i < Material::kTextureSlotCount; ++i) {
		Material::TextureSlot slot = static_cast<Material::TextureSlot>(i);
		bool found = false;
		for(const Material& material: materials) {
			if(has_texture_path(material, slot)) {
				found = true;
				break;
			}
		}
		if(found)
			texture_slots.push_back(slot);
	}
	assert(base_mesh);
	for(MappedMesh *mesh: levels)
		delete mesh;
	levels.clear();

	nx::spatial_sort_mesh(*base_mesh);
	nx::compute_adjacency(*base_mesh);
	recompute_normals(*base_mesh);
	validate_normals_for_mapped_mesh(*base_mesh, "initial");

	levels.push_back(base_mesh);

	// Save list of levels
	try {
		save_levels_list(levels);
	} catch (const std::exception &e) {
		nx::log << "Warning: could not save levels list: " << e.what() << std::endl;
	}
}

void MeshHierarchy::build_hierarchy(const BuildParameters& params) {
	nx::log << "Building initial clusters and micronodes..." << std::endl;

	std::size_t max_triangles = params.faces_per_cluster;
	assert(levels.size() > 0);
	MappedMesh &mesh = *levels[0];


	for(size_t i = 0; i < mesh.wedges.size(); i++) {
		Wedge &w = mesh.wedges[i];
		assert(w.p < mesh.positions.size());
		assert(w.t < mesh.texcoords.size());
		assert(w.n < mesh.normals.size());
	}

	// Use explicit resume flag: when resuming skip initial clustering.
	if (!params.resume) {
		nx::build_initial_clusters(mesh, max_triangles*params.clusters_per_node,
					   params.use_greedy ? nx::ClusteringMethod::Greedy : nx::ClusteringMethod::Metis);

		//split each cluster in N clusters and create a micronode
		nx::split_initial_clusters(mesh, max_triangles);

		// Reparametrize all clusters after initial split
		reparametrize_initial_clusters(mesh, materials);

		// Save state after initial clustering / reparametrization
		try {
			mesh.saveState(mesh.dir / "state.json");
			nx::log << "Saved post-initialization state to " << (mesh.dir / "state.json") << std::endl;
		} catch (const std::exception &e) {
			nx::log << "Warning: could not save post-initialization state: " << e.what() << std::endl;
		}
	} else {
		nx::log << "Resume requested: skipping initial clustering." << std::endl;
	}


	for(size_t i = 0; i < mesh.wedges.size(); i++) {
		Wedge &w = mesh.wedges[i];
		assert(w.p < mesh.positions.size());
		assert(w.t < mesh.texcoords.size());
		assert(w.n < mesh.normals.size());
	}


	//parametrize and project texture
	if(0) {
		const std::filesystem::path out_dir = std::filesystem::current_path();
		const std::string level_tag = "level_0";
		export_ply(mesh, out_dir / (level_tag + "_clusters.ply"), ColoringMode::ByCluster);
		export_ply(mesh, out_dir / (level_tag + "_nodes.ply"), ColoringMode::ByMicroNode);
	}
	
	while (true) {
		MappedMesh &current = *levels.back();
		nx::log << "\n--- Level " << levels.size() << " ---" << std::endl;
		nx::log << "Micronodes: " << current.micronodes.size() << std::endl;
		nx::log << "Triangles: " << current.triangles.size() << std::endl;

		if(current.micronodes.size() == 1)
			break;
		
		MappedMesh *next_level = new MappedMesh();
		process_level(current, *next_level, params);
		
		levels.push_back(next_level);

		// Save state for the newly created level
		try {
			int level_index = static_cast<int>(levels.size()) - 1;
			next_level->saveState(next_level->dir / "state.json");
			nx::log << "Saved state for level " << level_index << " to " << (next_level->dir / "state.json") << std::endl;
		} catch (const std::exception &e) {
			nx::log << "Warning: could not save state for new level: " << e.what() << std::endl;
		}

		// Update levels list JSON
		try {
			save_levels_list(levels);
		} catch (const std::exception &e) {
			nx::log << "Warning: could not save levels list: " << e.what() << std::endl;
		}
	}
}

void MeshHierarchy::process_level(MappedMesh& mesh, MappedMesh& next_mesh, const BuildParameters &params) {
	static int current_level = 1;
	// Copy geometry (positions/wedges/colors/material_ids)
	//TODO copy is not needed...
	copyVertices(mesh, next_mesh);
	next_mesh.has_colors = mesh.has_colors;
	next_mesh.has_normals = mesh.has_normals;
	next_mesh.has_textures = mesh.has_textures;

	//TODO when implementing macrfonodes, we should use the size of the macronodes
	//to keep the amount of texture reasonable (on top of this).
	bool high_triangle_texel_ratio = false;
	if(mesh.has_textures && !mesh.node_textures.empty() && !mesh.micronodes.empty()) {
		const int previous_tex_res = mesh.node_textures[0].width;
		assert(previous_tex_res > 0);
		const double texels_per_micronode = static_cast<double>(previous_tex_res) * static_cast<double>(previous_tex_res);
		assert(texels_per_micronode > 0.0);
		const double triangles_per_micronode = static_cast<double>(mesh.triangles.size()) / static_cast<double>(mesh.micronodes.size());
		const double triangle_texel_ratio = texels_per_micronode/ triangles_per_micronode;
		high_triangle_texel_ratio = triangle_texel_ratio > static_cast<double>(params.triangle_texel_ratio);
	}

	std::mutex next_mesh_lock;
	dp::thread_pool pool;

	// Now iterate on the current mesh micronodes to merge, simplify, and split
	for(Index micro_id = 0; micro_id < mesh.micronodes.size(); micro_id++) {


		// add tasks, in this case without caring about results of individual tasks
		pool.enqueue_detach([this, &mesh, &next_mesh, &params, &next_mesh_lock, high_triangle_texel_ratio](Index micro_id) {
			MicroNode& micronode = mesh.micronodes[micro_id];


			// 1) Collect its clusters into a temporary mesh and lock boundaries.
			NodeMesh simplified = merge_micronode_clusters_for_simplification(mesh, micronode); //drop normals and textures.

			// 2) Simplify node triangles (vertex collapse moves positions in new mesh)
			const float simplification_ratio = (mesh.has_textures && high_triangle_texel_ratio) ? 0.9f : params.scaling;
			const Index target_triangle_count = std::max<Index>(1,
				static_cast<Index>(std::ceil(static_cast<float>(simplified.triangles.size()) * simplification_ratio)));
			//simplify_mesh_edge(merged, target_triangle_count);
			micronode.error = simplify_mesh(simplified, target_triangle_count);

			{
				const std::lock_guard<std::mutex> lock(next_mesh_lock);

				//tempoarily keep the dependencies in micronode.children_nodes
				split_mesh(simplified, micro_id, next_mesh, params.faces_per_cluster);
			}

			if(0){
				MappedMesh debug_mesh;
				debug_mesh.positions.resize(simplified.positions.size());
				for (Index i = 0; i < simplified.positions.size(); ++i) {
					debug_mesh.positions[i] = simplified.positions[i];
				}
				debug_mesh.wedges.resize(simplified.wedges.size());
				for (Index i = 0; i < simplified.wedges.size(); ++i) {
					debug_mesh.wedges[i] = simplified.wedges[i];
				}
				debug_mesh.triangles.resize(simplified.triangles.size());
				for (Index i = 0; i < simplified.triangles.size(); ++i) {
					debug_mesh.triangles[i] = simplified.triangles[i];
				}
				export_obj(debug_mesh, materials, "debug_clustered.obj");
			}
		}, micro_id);
		pool.wait_for_tasks();

	}


	// Recompute adjacency for the next level
	if (next_mesh.triangles.size() > 0) {
		compute_adjacency(next_mesh);
	}

	recompute_normals(next_mesh);

	//TODO sort clusters  (including triangles) by center to improve locality?


// Returns a vector of MicroNode structures representing the partitions.
	next_mesh.micronodes = create_micronodes_metis(next_mesh, params.clusters_per_node, params.faces_per_cluster);
	if (next_mesh.has_textures) {
		reparametrize_clusters(mesh, next_mesh, texture_slots, materials, high_triangle_texel_ratio);
	}

	for(size_t i = 0; i < next_mesh.wedges.size(); i++) {
		Wedge &w = next_mesh.wedges[i];
		assert(w.p < next_mesh.positions.size());
		assert(w.t < next_mesh.texcoords.size());
		assert(w.n < next_mesh.normals.size());
	}
	//TODO make sure normals are empty until we compute them.

	// export to the ply by cluster and by node
	if(0){
		const std::filesystem::path out_dir = std::filesystem::current_path();
		const std::string level_tag = "level_" + std::to_string(current_level);
		export_ply(next_mesh, out_dir / (level_tag + "_clusters.ply"), ColoringMode::ByCluster);
		export_ply(next_mesh, out_dir / (level_tag + "_nodes.ply"), ColoringMode::ByMicroNode);
	}
	current_level++;
}

} // namespace nx
