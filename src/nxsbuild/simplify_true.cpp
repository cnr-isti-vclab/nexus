#include "merge_simplify_split.h"
#include "log.h"

#ifndef USE_VCG_SIMPLIFIER

#include <cstdint>
#include <stdexcept>
#include <vector>

#include <trueform/remesh.hpp>

namespace nx {

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
	config.parallel = true;
	config.preserve_boundary = false;
	float target_ratio = static_cast<float>(target_triangle_count) / merged.triangles.size();
	auto [result, half_edges, protection, face_map, vertex_map] = tf::decimated(
		polygons.polygons(), target_ratio, config,
		tf::protect_vertices(pinned_vertices), tf::return_index_map);
	(void)half_edges;

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

	
	if(result.faces().size() > target_triangle_count*1.05) {
		nx::debug << "Failed to simplify cluster: target triangle count not reached. Target: " << target_triangle_count << " Result: " << merged.triangles.size();
		float error	= simplify_mesh_edge(merged, target_triangle_count);
		if(merged.triangles.size() > target_triangle_count*1.05) {
			throw std::runtime_error("Failed to simplify cluster: target triangle count not reached");
		}
		
	}
	return NodeMesh::meshError(merged);
}

}

#endif
