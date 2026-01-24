#ifndef NX_MICRO_CLUSTERING_METIS_H
#define NX_MICRO_CLUSTERING_METIS_H

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
											   std::size_t clusters_per_micronode = 8);

// Build both primary and dual micronodes simultaneously using METIS partitioning.
// Primary micronodes: Partition of full cluster graph
// Dual micronodes: Partition of cluster graph with internal primary micronode edges removed
// 
// Returns pair<primary_micronodes, dual_micronodes>
std::pair<std::vector<MicroNode>, std::vector<MicroNode>> 
create_primary_and_dual_micronodes(const MeshFiles& mesh,
								   std::size_t clusters_per_micronode = 8);

// Measure overlap between primary and dual micronode partitions.
// For each primary micronode, counts shared edges with each dual micronode.
// Reports fraction of primary micronodes that share 3+ edges with at least one dual micronode.
// 
// Returns the fraction [0.0, 1.0]
float measure_micronode_overlap(const MeshFiles& mesh,
								const std::vector<MicroNode>& primary_micronodes,
								const std::vector<MicroNode>& dual_micronodes);

// Recluster the mesh using multi-constraint METIS partitioning.
// Takes primary and dual micronode partitions and creates new clusters by:
// - Using N constraints (one per original cluster)
// - Creating M partitions where each partition draws triangles from 4 source clusters
// - Each new partition gets ~50% of triangles from each of its 4 source clusters
// 
// Returns new cluster assignments for each triangle
std::vector<Cluster> recluster_mesh_multiconstraint(
	MeshFiles& mesh,
	const std::vector<MicroNode>& primary_micronodes,
	const std::vector<MicroNode>& dual_micronodes,
	std::size_t target_triangles_per_cluster = 128);

// Build primary/dual micronodes and recluster the mesh in one step.
// Combines create_primary_and_dual_micronodes, measure_micronode_overlap,
// and recluster_mesh_multiconstraint.
//
// mesh: MeshFiles with clusters already computed (will be modified in-place)
// clusters_per_micronode: Target number of clusters per micronode
// target_triangles_per_cluster: Target triangles per cluster after reclustering
void build_dual_partition_and_recluster(
	MeshFiles& mesh,
	std::size_t clusters_per_micronode = 4,
	std::size_t target_triangles_per_cluster = 128);

} // namespace nx

#endif // NX_MICRO_CLUSTERING_METIS_H
