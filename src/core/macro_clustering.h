#ifndef NX_MACRO_CLUSTERING_H
#define NX_MACRO_CLUSTERING_H

#include "mesh_types.h"
#include <cstddef>
#include <vector>
#include <set>
#include <queue>

namespace nx {

class MeshFiles;

// Dual graph representation of micro-nodes (clusters) in CSR format.
// Used for macro-node partitioning.
struct MacroGraph {
    std::vector<MicroNode> nodes;        // Micro-node metadata
    std::vector<Index> xadj;             // CSR index array (adjacency starts at xadj[i])
    std::vector<Index> adjncy;           // CSR adjacency array (neighboring micro-node IDs)
    std::vector<Index> edgeW;            // Edge weights (shared triangle count between micro-nodes)
};

// Represents one partition cell (macro-node) during refinement.
struct PartitionCell {
    std::vector<Index> micro_nodes;      // Micro-node IDs in this cell
    Index triangle_count;                // Total triangles
    Aabb bounds;                         // Union of micro-node bounds
    std::set<Index> parent_intervals;    // Parent macro-node IDs (for WebGL hierarchy)
};

// Build dual graph from micro-nodes (clusters).
// Computes shared edges between clusters and builds CSR adjacency.
MacroGraph build_dual_graph(const std::vector<MicroNode>& micro_nodes,
                            const MappedArray<Triangle>& triangles,
                            const MappedArray<FaceAdjacency>& adjacency);

// Phase 1: Simultaneous greedy growth with multi-seeds.
// Partitions micro-nodes into K macro-nodes using topology-aware growth.
std::vector<PartitionCell> phase1_simultaneous_growth(
    const MacroGraph& graph,
    Index num_macro_nodes,
    float distance_weight = 0.1f);

// Phase 2: KL-FM refinement with bi-objective optimization.
// Refines macro-node partition to balance topology and spatial compactness.
void phase2_kl_fm_refinement(
    std::vector<PartitionCell>& partition,
    const MacroGraph& graph,
    float topo_weight = 1.0f,
    float spatial_weight = 0.5f,
    Index max_iterations = 10);

} // namespace nx

#endif // NX_MACRO_CLUSTERING_H
