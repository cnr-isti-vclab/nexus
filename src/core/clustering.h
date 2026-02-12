#ifndef NX_CLUSTERING_H
#define NX_CLUSTERING_H

#include <cstddef>
#include "mesh_types.h"
#include "mapped_file.h"

namespace nx {

enum class ClusteringMethod {
	Greedy,
	Metis
};

class MappedMesh;


// Build clusters using the selected method (greedy or METIS).
void build_clusters(MappedMesh& mesh, std::size_t max_triangles, ClusteringMethod method);

//TODO: we need to measure the quality of clustering (border length, compactness, etc)

// Build clusters from triangles using a greedy approach with boundary minimization.
// Based on Nanite principles as implemented in meshoptimizer.
// 
// max_triangles: Maximum triangles per cluster (typically 126 for NVidia, 64 for AMD)
// 
// Returns the number of clusters created.
// Clusters with bounds are written to mesh.clusters.
void build_clusters_greedy(MappedMesh& mesh, std::size_t max_triangles);


// Build clusters from triangles using METIS graph partitioning.
// METIS provides high-quality k-way partitioning optimizing edge cut.
// 
// max_triangles: Maximum triangles per cluster (typically 126 for NVidia, 64 for AMD)
// 
// Returns the number of clusters created.
// Clusters with bounds are written to mesh.clusters.
void build_clusters_metis(MappedMesh& mesh, std::size_t max_triangles);

void reorder_triangles_by_cluster(MappedMesh& mesh);
void compute_cluster_bounds(MappedMesh& mesh);
void print_cluster_histogram(const MappedArray<Cluster>& clusters, const char* label = "");
Vector3f compute_triangle_centroid(const MappedArray<Vector3f>& positions,
								   const MappedArray<Wedge>& wedges,
								   const Triangle& tri);

// Split each cluster into 4 smaller clusters and create a micronode for each group.
// Each original cluster becomes a micronode containing its 4 child clusters.
// max_triangles: target maximum triangles per resulting cluster
void split_clusters(MappedMesh& mesh, std::size_t max_triangles);

} // namespace nx

#endif
