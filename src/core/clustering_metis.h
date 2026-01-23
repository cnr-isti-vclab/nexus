#ifndef NX_CLUSTERING_METIS_H
#define NX_CLUSTERING_METIS_H

#include <cstddef>

namespace nx {

class MeshFiles;

// Build clusters from triangles using METIS graph partitioning.
// METIS provides high-quality k-way partitioning optimizing edge cut.
// 
// max_triangles: Maximum triangles per cluster (typically 126 for NVidia, 64 for AMD)
// 
// Returns the number of clusters created.
// Clusters with bounds are written to mesh.clusters.
std::size_t build_clusters_metis(MeshFiles& mesh, std::size_t max_triangles);

} // namespace nx

#endif // NX_CLUSTERING_METIS_H
