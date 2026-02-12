#pragma once

#include <set>
#include <unordered_map>
#include <vector>

#include "../core/nodemesh.h"
#include "../core/material.h"
namespace nx {

class MappedMesh;


// Step 6: Result of splitting a simplified mesh
struct SplitResult {
	std::vector<Index> cluster_assignment;  // Triangle -> cluster ID (0 or 1)
	Index cluster_0_count;
	Index cluster_1_count;
};

NodeMesh merge_micronode_clusters_for_simplification(const MappedMesh& mesh,
													   const MicroNode& micronode);



// Merge 4 clusters from a micronode into a single mesh
NodeMesh merge_micronode_clusters(const MappedMesh& mesh,
									const MicroNode& micronode);


// Simplify mesh to target triangle count (respecting locked boundaries)
float simplify_mesh(NodeMesh &mesh, Index target_triangle_count);

float simplify_mesh_edge(
	NodeMesh& mesh,
	Index target_triangle_count);

	
// Update the positions (and potentially wedges) in next_mesh using the simplified merged mesh.
// The mapping is from original position index -> merged position index.
void update_vertices_and_wedges(
	MappedMesh& next_mesh,
	const NodeMesh& merged);

// Split a simplified merged mesh into clusters (without micronode assignment).
// Records the created cluster indices in micronode.children_nodes (temporary storage).
// The clusters are split based on max_triangles target.
// Returns the indices of the newly created clusters.
void split_mesh(const NodeMesh& merged,
	Index micro_id,
	MappedMesh& next_mesh,
	std::size_t max_triangles);

}
