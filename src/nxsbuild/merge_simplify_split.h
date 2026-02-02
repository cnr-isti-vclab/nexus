#pragma once

#include <set>
#include <unordered_map>
#include <vector>

#include "../core/mesh_types.h"
namespace nx {

class MeshFiles;

// Step 3: Merge clusters within a micronode into a temporary mesh
struct MergedMesh {
	std::vector<Vector3f> positions;
	std::vector<Wedge> wedges;
	std::vector<Triangle> triangles;
	std::vector<Index> boundary_vertices;  // Vertices on micronode boundary
	std::unordered_map<Index, Index> position_map; // original position -> merged position
	std::vector<Index> position_remap;//merged_position -> original position
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
	const MicroNode& micronode);


// Simplify mesh to target triangle count (respecting locked boundaries)
float simplify_mesh(
	MergedMesh& mesh,
	Index target_triangle_count);

float simplify_mesh_edge(
	MergedMesh& mesh,
	Index target_triangle_count);

	
// Update the positions (and potentially wedges) in next_mesh using the simplified merged mesh.
// The mapping is from original position index -> merged position index.
void update_vertices_and_wedges(
	MeshFiles& next_mesh,
	const MergedMesh& merged);

// Split a simplified merged mesh into clusters (without micronode assignment).
// Records the created cluster indices in micronode.children_nodes (temporary storage).
// The clusters are split based on max_triangles target.
// Returns the indices of the newly created clusters.
void split_mesh(const MergedMesh& merged,
	Index micro_id,
	MeshFiles& next_mesh,
	std::size_t max_triangles);

}
