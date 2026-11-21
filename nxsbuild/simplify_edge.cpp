#include "merge_simplify_split.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <set>
#include <stdexcept>
#include <utility>
#include <vector>

namespace nx {
namespace {

struct EdgeCandidate {
	Index keep;
	Index remove;
	double length_squared;
};

} // namespace

float simplify_mesh_edge(NodeMesh& mesh, Index target_triangle_count) {
	if(mesh.triangles.size() <= target_triangle_count || target_triangle_count == 0)
		return NodeMesh::meshError(mesh);

	if(!mesh.material_ids.empty() && mesh.material_ids.size() != mesh.triangles.size())
		throw std::runtime_error("Edge simplification: material count does not match triangle count");

	std::vector<uint8_t> boundary(mesh.positions.size(), 0);
	for(Index vertex : mesh.boundary_vertices) {
		if(vertex >= boundary.size())
			throw std::runtime_error("Edge simplification: boundary vertex index is out of range");
		boundary[vertex] = 1;
	}

	for(const Triangle& triangle : mesh.triangles) {
		for(Index wedge : triangle.w) {
			if(wedge >= mesh.wedges.size() || mesh.wedges[wedge].p >= mesh.positions.size())
				throw std::runtime_error("Edge simplification: invalid triangle/wedge/position index");
		}
	}

	while(mesh.triangles.size() > target_triangle_count) {
		std::set<std::pair<Index, Index>> seen_edges;
		EdgeCandidate shortest{0, 0, std::numeric_limits<double>::infinity()};
		bool found_edge = false;

		for(const Triangle& triangle : mesh.triangles) {
			for(int corner = 0; corner < 3; ++corner) {
				Index a = mesh.wedges[triangle.w[corner]].p;
				Index b = mesh.wedges[triangle.w[(corner + 1) % 3]].p;
				if(a == b)
					continue;
				if(a > b)
					std::swap(a, b);
				if(!seen_edges.emplace(a, b).second || boundary[a] || boundary[b])
					continue;

				const Vector3f& pa = mesh.positions[a];
				const Vector3f& pb = mesh.positions[b];
				const double dx = double(pa.x) - pb.x;
				const double dy = double(pa.y) - pb.y;
				const double dz = double(pa.z) - pb.z;
				const double length_squared = dx*dx + dy*dy + dz*dz;
				if(!std::isfinite(length_squared))
					throw std::runtime_error("Edge simplification: encountered a non-finite edge length");
				if(length_squared < shortest.length_squared) {
					shortest = {a, b, length_squared};
					found_edge = true;
				}
			}
		}

		if(!found_edge)
			throw std::runtime_error("Edge simplification: no collapsible edge remains before reaching target triangle count");

		Vector3f& kept_position = mesh.positions[shortest.keep];
		const Vector3f& removed_position = mesh.positions[shortest.remove];
		kept_position = {(kept_position.x + removed_position.x) * 0.5f,
		                 (kept_position.y + removed_position.y) * 0.5f,
		                 (kept_position.z + removed_position.z) * 0.5f};

		for(Wedge& wedge : mesh.wedges)
			if(wedge.p == shortest.remove)
				wedge.p = shortest.keep;

		std::vector<Triangle> kept_triangles;
		kept_triangles.reserve(mesh.triangles.size());
		std::vector<Index> kept_material_ids;
		if(!mesh.material_ids.empty())
			kept_material_ids.reserve(mesh.material_ids.size());
		for(std::size_t i = 0; i < mesh.triangles.size(); ++i) {
			const Triangle& triangle = mesh.triangles[i];
			const Index p0 = mesh.wedges[triangle.w[0]].p;
			const Index p1 = mesh.wedges[triangle.w[1]].p;
			const Index p2 = mesh.wedges[triangle.w[2]].p;
			if(p0 == p1 || p1 == p2 || p0 == p2)
				continue;
			kept_triangles.push_back(triangle);
			if(!mesh.material_ids.empty())
				kept_material_ids.push_back(mesh.material_ids[i]);
		}
		mesh.triangles.swap(kept_triangles);
		if(!mesh.material_ids.empty())
			mesh.material_ids.swap(kept_material_ids);
	}

	return NodeMesh::meshError(mesh);
}

} // namespace nx
