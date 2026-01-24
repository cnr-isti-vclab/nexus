#include "adjacency.h"
#include <algorithm>
#include <iostream>

namespace nx {

// Border marker
static constexpr Index BORDER = UINT32_MAX;

void compute_adjacency(MeshFiles& mesh) {
	Index num_triangles = mesh.triangles.size();
	std::cout << "Computing adjacency for " << num_triangles << " triangles..." << std::endl;

	if (num_triangles == 0) {
		return;
	}

	// Create halfedge list (3 halfedges per triangle)
	std::vector<HalfedgeRecord> halfedges;
	halfedges.reserve(num_triangles * 3);

	for (Index face_idx = 0; face_idx < num_triangles; ++face_idx) {
		const Triangle& tri = mesh.triangles[face_idx];

		// Extract position indices from wedges
		Index p[3];
		for (int i = 0; i < 3; ++i) {
			p[i] = mesh.wedges[tri.w[i]].p;
		}

		// Create 3 halfedges for this triangle
		for (int corner = 0; corner < 3; ++corner) {
			HalfedgeRecord he;
			he.v0 = p[corner];
			he.v1 = p[(corner + 1) % 3];
			he.face = face_idx;
			he.corner = corner;
			halfedges.push_back(he);
		}
	}

	std::cout << "Created " << halfedges.size() << " halfedges, sorting..." << std::endl;

	// Sort halfedges by (min(v0,v1), max(v0,v1)) to group edge pairs together
	std::sort(halfedges.begin(), halfedges.end(), [](const HalfedgeRecord& a, const HalfedgeRecord& b) {
		Index a_min = std::min(a.v0, a.v1);
		Index a_max = std::max(a.v0, a.v1);
		Index b_min = std::min(b.v0, b.v1);
		Index b_max = std::max(b.v0, b.v1);

		if (a_min != b_min) return a_min < b_min;
		if (a_max != b_max) return a_max < b_max;

		// Secondary sort by v0 to separate (v0,v1) from (v1,v0)
		return a.v0 < b.v0;
	});

	std::cout << "Building adjacency array..." << std::endl;

	// Resize and initialize adjacency array
	mesh.adjacency.resize(num_triangles);
	for (Index i = 0; i < num_triangles; ++i) {
		for (int j = 0; j < 3; ++j) {
			mesh.adjacency[i].opp[j] = BORDER;
		}
	}

	// Match opposite halfedges
	size_t i = 0;
	while (i < halfedges.size()) {
		const HalfedgeRecord& he = halfedges[i];
		Index edge_min = std::min(he.v0, he.v1);
		Index edge_max = std::max(he.v0, he.v1);

		// Find all halfedges sharing this edge
		size_t j = i;
		while (j < halfedges.size()) {
			const HalfedgeRecord& other = halfedges[j];
			Index other_min = std::min(other.v0, other.v1);
			Index other_max = std::max(other.v0, other.v1);

			if (other_min != edge_min || other_max != edge_max) {
				break;
			}
			j++;
		}

		size_t edge_count = j - i;

		if (edge_count == 2) {
			// Manifold edge: match the two opposite halfedges
			const HalfedgeRecord& he0 = halfedges[i];
			const HalfedgeRecord& he1 = halfedges[i + 1];

			// Verify they are opposite directions
			if ((he0.v0 == he1.v1) && (he0.v1 == he1.v0)) {
				mesh.adjacency[he0.face].opp[he0.corner] = he1.face;
				mesh.adjacency[he1.face].opp[he1.corner] = he0.face;
			}
		} else if (edge_count > 2) {
			// Non-manifold: treat all as border
			std::cout << "Warning: Non-manifold edge (" << edge_min << ", " << edge_max
					  << ") with " << edge_count << " incident faces" << std::endl;
		}
		// If edge_count == 1, it's already a border (initialized to BORDER)

		i = j;
	}

	// Count border edges
	Index border_count = 0;
	for (Index i = 0; i < num_triangles; ++i) {
		for (int j = 0; j < 3; ++j) {
			if (mesh.adjacency[i].opp[j] == BORDER) {
				border_count++;
			}
		}
	}

	std::cout << "Adjacency complete: " << border_count << " border halfedges" << std::endl;
}

} // namespace nx
