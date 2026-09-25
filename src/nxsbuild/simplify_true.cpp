#include "merge_simplify_split.h"

#ifndef USE_VCG_SIMPLIFIER

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <trueform/remesh.hpp>

namespace nx {

namespace {

float meshError(NodeMesh& mesh) {
	double length = 0;
	for (const Triangle& triangle : mesh.triangles) {
		Index p0 = mesh.wedges[triangle.w[0]].p;
		Index p1 = mesh.wedges[triangle.w[1]].p;
		Index p2 = mesh.wedges[triangle.w[2]].p;
		const Vector3f& a = mesh.positions[p0];
		const Vector3f& b = mesh.positions[p1];
		const Vector3f& c = mesh.positions[p2];
		float dx01 = a.x - b.x; float dy01 = a.y - b.y; float dz01 = a.z - b.z;
		float dx12 = b.x - c.x; float dy12 = b.y - c.y; float dz12 = b.z - c.z;
		float dx20 = c.x - a.x; float dy20 = c.y - a.y; float dz20 = c.z - a.z;
		length += dx01 * dx01 + dy01 * dy01 + dz01 * dz01;
		length += dx12 * dx12 + dy12 * dy12 + dz12 * dz12;
		length += dx20 * dx20 + dy20 * dy20 + dz20 * dz20;
	}
	length = std::sqrt(length) / mesh.triangles.size() * 3;
	return static_cast<float>(length);
}

}

float simplify_mesh(NodeMesh& merged, Index target_triangle_count) {
	if (merged.triangles.size() <= target_triangle_count || target_triangle_count == 0) {
		return 0;
	}

	tf::polygons_buffer<std::int32_t, float, 3, 3> polygons;
	for (const Vector3f& position : merged.positions) {
		polygons.points_buffer().emplace_back(position.x, position.y, position.z);
	}
	for (const Triangle& triangle : merged.triangles) {
		polygons.faces_buffer().emplace_back(
			static_cast<std::int32_t>(merged.wedges[triangle.w[0]].p),
			static_cast<std::int32_t>(merged.wedges[triangle.w[1]].p),
			static_cast<std::int32_t>(merged.wedges[triangle.w[2]].p));
	}

	std::vector<int> pinned_vertices(merged.positions.size(), 0);
	for (Index vertex : merged.boundary_vertices) {
		pinned_vertices[vertex] = 1;
	}

	tf::decimate_config<float> config;
	config.parallel = false;
	config.preserve_boundary = true;
	float target_ratio = static_cast<float>(target_triangle_count) / merged.triangles.size();
	auto [result, half_edges, protection, face_map, vertex_map] = tf::decimated(
		polygons.polygons(), target_ratio, config,
		tf::protect_vertices(pinned_vertices), tf::return_index_map);
	(void)half_edges;

	if(result.faces().size() > target_triangle_count*1.05) {
		int triangle_count = result.faces().size();
		throw std::string("Failed to simplify cluster");
	}

	const std::vector<Index> original_position_remap = merged.position_remap;
	const std::vector<Rgba8> original_colors = merged.colors;
	const std::vector<Index> original_material_ids = merged.material_ids;

	merged.positions.clear();
	merged.positions.reserve(result.points().size());
	for (auto point : result.points()) {
		merged.positions.push_back({point[0], point[1], point[2]});
	}

	merged.position_remap.resize(vertex_map.kept_ids().size());
	merged.colors.clear();
	if (!original_colors.empty()) {
		merged.colors.reserve(vertex_map.kept_ids().size());
	}
	for (std::size_t i = 0; i < vertex_map.kept_ids().size(); ++i) {
		Index old_vertex = vertex_map.kept_ids()[i];
		merged.position_remap[i] = original_position_remap[old_vertex];
		if (!original_colors.empty()) {
			merged.colors.push_back(original_colors[old_vertex]);
		}
	}

	for (Index& vertex : merged.boundary_vertices) {
		std::int32_t new_vertex = vertex_map.f()[vertex];
		assert(new_vertex >= 0 && static_cast<std::size_t>(new_vertex) < vertex_map.f().size());
		assert(protection[static_cast<std::size_t>(new_vertex)]);
		vertex = static_cast<Index>(new_vertex);
	}

	merged.position_map.clear();
	for (Index vertex = 0; vertex < merged.position_remap.size(); ++vertex) {
		merged.position_map[merged.position_remap[vertex]] = vertex;
	}

	merged.triangles.clear();
	merged.triangles.reserve(result.faces().size());
	for (auto face : result.faces()) {
		auto [v0, v1, v2] = face;
		Triangle triangle;
		triangle.w[0] = static_cast<Index>(v0);
		triangle.w[1] = static_cast<Index>(v1);
		triangle.w[2] = static_cast<Index>(v2);
		merged.triangles.push_back(triangle);
	}

	merged.wedges.resize(merged.positions.size());
	for (std::size_t i = 0; i < merged.wedges.size(); ++i) {
		merged.wedges[i].p = static_cast<Index>(i);
		merged.wedges[i].n = NONE;
		merged.wedges[i].t = NONE;
	}

	merged.material_ids.clear();
	if (!original_material_ids.empty()) {
		merged.material_ids.resize(face_map.kept_ids().size());
		for (std::size_t i = 0; i < face_map.kept_ids().size(); ++i) {
			merged.material_ids[i] = original_material_ids[face_map.kept_ids()[i]];
		}
	}

	return meshError(merged);
}

}

#endif
