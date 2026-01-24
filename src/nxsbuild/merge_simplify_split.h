#pragma once

#include <set>
#include <vector>

#include "../core/mesh_types.h"

namespace nx {

class MeshFiles;

// Step 3: Merge clusters within a micronode into a temporary mesh
struct MergedMesh {
	std::vector<Vector3f> positions;
	std::vector<Wedge> wedges;
	std::vector<Triangle> triangles;
	std::set<Index> boundary_vertices;  // Vertices on micronode boundary
	Index parent_micronode_id;
};

// Step 6: Result of splitting a simplified mesh
struct SplitResult {
	std::vector<Index> cluster_assignment;  // Triangle -> cluster ID (0 or 1)
	Index cluster_0_count;
	Index cluster_1_count;
};

// Merge 4 clusters from a micronode into a single mesh
MergedMesh merge_micronode_clusters(
	const MeshFiles& mesh,
	const MicroNode& micronode,
	const std::set<Index>& external_clusters);

// Identify and lock boundary edges (edges connecting to external clusters)
void identify_boundary_vertices(
	MergedMesh& merged,
	const MeshFiles& mesh,
	const MicroNode& micronode,
	const std::set<Index>& external_clusters);

// Simplify mesh to target triangle count (respecting locked boundaries)
void simplify_mesh(
	MergedMesh& mesh,
	Index target_triangle_count);

// Split simplified mesh into 2 clusters minimizing external edges
SplitResult split_into_two_clusters(
	const MergedMesh& mesh,
	const std::set<Index>& external_cluster_boundaries);

} // namespace nx
