#ifndef NX_CLUSTERING_H
#define NX_CLUSTERING_H

#include <cstddef>
#include "mesh_types.h"
#include "mapped_file.h"

namespace nx {

class MeshFiles;

//TODO: we need to measure the quality of clustering (border length, compactness, etc)

// Build clusters from triangles using a greedy approach with boundary minimization.
// Based on Nanite principles as implemented in meshoptimizer.
// 
// max_triangles: Maximum triangles per cluster (typically 126 for NVidia, 64 for AMD)
// 
// Returns the number of clusters created.
// Clusters with bounds are written to mesh.clusters.
std::size_t build_clusters(MeshFiles& mesh, std::size_t max_triangles);

void reorder_triangles_by_cluster(MeshFiles& mesh);
void compute_cluster_bounds(MeshFiles& mesh);
void print_cluster_histogram(const MappedArray<Cluster>& clusters, const char* label = "");
Vector3f compute_triangle_centroid(const MappedArray<Vector3f>& positions,
                                  const MappedArray<Wedge>& wedges,
                                  const Triangle& tri);

} // namespace nx

#endif
