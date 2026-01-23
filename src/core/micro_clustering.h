#ifndef NX_MICRO_CLUSTERING_H
#define NX_MICRO_CLUSTERING_H

#include <vector>
#include "mesh_types.h"

namespace nx {

class MeshFiles;

// Build micronodes from clusters using greedy region growing with boundary minimization.
// Mirrors the clustering algorithm but works on clusters instead of triangles.
// 
// mesh: MeshFiles with clusters already computed
// clusters_per_micronode: Target number of clusters per micronode (default 8)
// 
// Returns a vector of MicroNode structures representing the partitions.
std::vector<MicroNode> create_micronodes(const MeshFiles& mesh,
                                         std::size_t clusters_per_micronode = 8);

} // namespace nx

#endif
