#ifndef NX_CLUSTERING_H
#define NX_CLUSTERING_H

#include <cstddef>

namespace nx {

class MeshFiles;

// Build clusters from triangles using a greedy approach with boundary minimization.
// Based on Nanite principles as implemented in meshoptimizer.
// 
// max_triangles: Maximum triangles per cluster (typically 126 for NVidia, 64 for AMD)
// 
// Returns the number of clusters created.
// Clusters are written to mesh.clusters, cluster bounds to mesh.cluster_bounds.
std::size_t build_clusters(MeshFiles& mesh, std::size_t max_triangles);

} // namespace nx

#endif
