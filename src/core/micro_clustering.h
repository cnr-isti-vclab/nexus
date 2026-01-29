#ifndef NX_MICRO_CLUSTERING_H
#define NX_MICRO_CLUSTERING_H

#include <vector>
#include "mesh_types.h"

namespace nx {

class MeshFiles;

// Build micronodes from clusters using METIS graph partitioning.
// Edge weights are proportional to the number of shared triangle edges between clusters.
//
// mesh: MeshFiles with clusters already computed
// clusters_per_micronode: Target number of clusters per micronode (default 8)
//
// Returns a vector of MicroNode structures representing the partitions.
std::vector<MicroNode> create_micronodes_metis(const MeshFiles& mesh,
											   std::size_t clusters_per_micronode,
											   std::size_t faces_per_cluster);



} // namespace nx

#endif // NX_MICRO_CLUSTERING_H
