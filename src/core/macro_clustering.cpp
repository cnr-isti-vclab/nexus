#include "macro_clustering.h"
#include "mesh.h"
#include "mesh_types.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>
#include <unordered_map>
#include <set>

namespace nx {

namespace {

// Helper: Compute shared edges between two micro-nodes
Index compute_shared_edges(const MacroGraph& graph, Index u, Index v) {
    // Both u and v must be adjacent to count shared edges
    bool found = false;
    Index shared_count = 0;
    
    // Check if v is a neighbor of u
    for (Index j = graph.xadj[u]; j < graph.xadj[u + 1]; ++j) {
        if (graph.adjncy[j] == v) {
            shared_count = graph.edgeW[j];
            found = true;
            break;
        }
    }
    
    return shared_count;
}

// Helper: Compute distance between two micro-nodes (Euclidean centroid distance)
float compute_distance(const MicroNode& a, const MicroNode& b) {
    float dx = a.centroid.x - b.centroid.x;
    float dy = a.centroid.y - b.centroid.y;
    float dz = a.centroid.z - b.centroid.z;
    return std::sqrt(dx*dx + dy*dy + dz*dz);
}

// Helper: Merge two bounding spheres into one encompassing sphere
void merge_spheres(Vector3f& center_a, float& radius_a, const Vector3f& center_b, float radius_b) {
    float dx = center_b.x - center_a.x;
    float dy = center_b.y - center_a.y;
    float dz = center_b.z - center_a.z;
    float dist = std::sqrt(dx*dx + dy*dy + dz*dz);
    
    // If one sphere contains the other, use the larger one
    if (dist + radius_b <= radius_a) {
        return; // sphere_a already contains sphere_b
    }
    if (dist + radius_a <= radius_b) {
        center_a = center_b;
        radius_a = radius_b;
        return;
    }
    
    // Compute new sphere that encompasses both
    float new_radius = (radius_a + radius_b + dist) * 0.5f;
    if (dist > 0.0f) {
        float scale = (new_radius - radius_a) / dist;
        center_a.x += dx * scale;
        center_a.y += dy * scale;
        center_a.z += dz * scale;
    }
    radius_a = new_radius;
}

// Helper: Pick K seeds using MaxMin heuristic
std::vector<Index> select_seeds(const MacroGraph& graph, Index k) {
    if (k >= graph.nodes.size()) {
        // If k >= number of nodes, each node is its own seed
        std::vector<Index> seeds;
        for (Index i = 0; i < graph.nodes.size(); ++i) {
            seeds.push_back(i);
        }
        return seeds;
    }
    
    std::vector<Index> seeds;
    std::vector<bool> selected(graph.nodes.size(), false);
    
    // First seed: pick one (arbitrary, or boundary)
    seeds.push_back(0);
    selected[0] = true;
    
    // Subsequent seeds: pick furthest from all existing seeds
    for (Index s = 1; s < k; ++s) {
        float max_min_dist = -1.0f;
        Index best_idx = 0;
        
        for (Index i = 0; i < graph.nodes.size(); ++i) {
            if (selected[i]) continue;
            
            float min_dist = std::numeric_limits<float>::max();
            for (Index seed : seeds) {
                float d = compute_distance(graph.nodes[i], graph.nodes[seed]);
                min_dist = std::min(min_dist, d);
            }
            
            if (min_dist > max_min_dist) {
                max_min_dist = min_dist;
                best_idx = i;
            }
        }
        
        seeds.push_back(best_idx);
        selected[best_idx] = true;
    }
    
    return seeds;
}

} // anonymous namespace

MacroGraph build_dual_graph(const std::vector<MicroNode>& micro_nodes,
                            const MappedArray<Triangle>& triangles,
                            const MappedArray<FaceAdjacency>& adjacency) {
    MacroGraph graph;
    graph.nodes = micro_nodes;
    
    // Build adjacency: two micro-nodes are adjacent if they share a triangle edge
    std::unordered_map<Index, std::vector<std::pair<Index, Index>>> adj_list; // micro -> [(neighbor, shared_edges)]
    
    for (Index i = 0; i < micro_nodes.size(); ++i) {
        adj_list[i] = {};
    }
    
    // For each micro-node, find neighbors by checking triangle adjacencies
    // (This is a simplified version; in practice, you'd track edges between clusters)
    for (Index i = 0; i < micro_nodes.size(); ++i) {
        for (Index j = i + 1; j < micro_nodes.size(); ++j) {
            // Count shared boundary edges between cluster i and j
            // This requires knowing which triangles belong to each cluster
            // For now, we'll use a placeholder
            Index shared = 0;
            
            // TODO: Implement actual shared edge counting
            // This requires triangle-to-cluster mapping
            
            if (shared > 0) {
                adj_list[i].push_back({j, shared});
                adj_list[j].push_back({i, shared});
            }
        }
    }
    
    // Build CSR format
    graph.xadj.resize(micro_nodes.size() + 1);
    graph.xadj[0] = 0;
    
    Index edge_count = 0;
    for (Index i = 0; i < micro_nodes.size(); ++i) {
        for (auto& [neighbor, weight] : adj_list[i]) {
            graph.adjncy.push_back(neighbor);
            graph.edgeW.push_back(weight);
            edge_count++;
        }
        graph.xadj[i + 1] = edge_count;
    }
    
    return graph;
}

std::vector<PartitionCell> phase1_simultaneous_growth(
    const MacroGraph& graph,
    Index num_macro_nodes,
    float distance_weight) {
    
    Index num_micro = graph.nodes.size();
    std::vector<Index> assignment(num_micro, std::numeric_limits<Index>::max());
    
    // Select seeds
    std::vector<Index> seeds = select_seeds(graph, num_macro_nodes);
    
    // Priority queue per seed: (priority, micro-node ID)
    std::vector<std::priority_queue<std::pair<float, Index>>> pqs(seeds.size());
    
    // Initialize: assign seeds to their respective macro-nodes
    for (Index k = 0; k < seeds.size(); ++k) {
        assignment[seeds[k]] = k;
        
        // Push neighbors of seed into its priority queue
        for (Index j = graph.xadj[seeds[k]]; j < graph.xadj[seeds[k] + 1]; ++j) {
            Index neighbor = graph.adjncy[j];
            if (assignment[neighbor] == std::numeric_limits<Index>::max()) {
                float priority = graph.edgeW[j] - distance_weight * compute_distance(graph.nodes[neighbor], graph.nodes[seeds[k]]);
                pqs[k].push({priority, neighbor});
            }
        }
    }
    
    // Growth loop: round-robin assignment
    bool all_assigned = false;
    while (!all_assigned) {
        all_assigned = true;
        
        for (Index k = 0; k < seeds.size(); ++k) {
            while (!pqs[k].empty()) {
                auto [priority, micro] = pqs[k].top();
                pqs[k].pop();
                
                // Skip if already assigned
                if (assignment[micro] != std::numeric_limits<Index>::max()) {
                    continue;
                }
                
                // Assign to this macro-node
                assignment[micro] = k;
                
                // Push neighbors into this macro-node's queue
                for (Index j = graph.xadj[micro]; j < graph.xadj[micro + 1]; ++j) {
                    Index neighbor = graph.adjncy[j];
                    if (assignment[neighbor] == std::numeric_limits<Index>::max()) {
                        float priority = graph.edgeW[j] - distance_weight * compute_distance(graph.nodes[neighbor], graph.nodes[seeds[k]]);
                        pqs[k].push({priority, neighbor});
                    }
                }
                
                break; // Move to next macro-node
            }
            
            if (!pqs[k].empty()) {
                all_assigned = false;
            }
        }
    }
    
    // Build partition cells
    std::vector<PartitionCell> partition(seeds.size());
    for (Index i = 0; i < num_macro_nodes; ++i) {
        partition[i].center = graph.nodes[0].center;
        partition[i].radius = 0.0f;
    }
    
    for (Index micro = 0; micro < num_micro; ++micro) {
        Index macro = assignment[micro];
        if (macro != std::numeric_limits<Index>::max()) {
            partition[macro].micro_nodes.push_back(micro);
            partition[macro].triangle_count += graph.nodes[micro].triangle_count;
            merge_spheres(partition[macro].center, partition[macro].radius, 
                         graph.nodes[micro].center, graph.nodes[micro].radius);
        }
    }
    
    return partition;
}

void phase2_kl_fm_refinement(
    std::vector<PartitionCell>& partition,
    const MacroGraph& graph,
    float topo_weight,
    float spatial_weight,
    Index max_iterations) {
    
    Index num_macro = partition.size();
    std::vector<Index> assignment(graph.nodes.size());
    
    // Build reverse mapping: micro-node -> macro-node
    for (Index macro = 0; macro < num_macro; ++macro) {
        for (Index micro : partition[macro].micro_nodes) {
            assignment[micro] = macro;
        }
    }
    
    // KL-FM refinement passes
    for (Index iteration = 0; iteration < max_iterations; ++iteration) {
        std::vector<std::tuple<float, Index, Index>> move_log; // (gain, micro, target_macro)
        std::vector<bool> locked(graph.nodes.size(), false);
        
        // Collect boundary micro-nodes
        std::priority_queue<std::pair<float, Index>> boundary_heap;
        
        for (Index macro = 0; macro < num_macro; ++macro) {
            for (Index micro : partition[macro].micro_nodes) {
                // Check if micro-node is on boundary
                bool is_boundary = false;
                for (Index j = graph.xadj[micro]; j < graph.xadj[micro + 1]; ++j) {
                    Index neighbor = graph.adjncy[j];
                    if (assignment[neighbor] != macro) {
                        is_boundary = true;
                        break;
                    }
                }
                
                if (is_boundary) {
                    // Compute gain for moving to best neighbor macro-node
                    float best_gain = -std::numeric_limits<float>::max();
                    Index best_target = macro;
                    
                    // Find best target macro-node
                    std::set<Index> neighbor_macros;
                    for (Index j = graph.xadj[micro]; j < graph.xadj[micro + 1]; ++j) {
                        Index neighbor = graph.adjncy[j];
                        Index neighbor_macro = assignment[neighbor];
                        if (neighbor_macro != macro) {
                            neighbor_macros.insert(neighbor_macro);
                        }
                    }
                    
                    // For each neighboring macro-node, compute gain
                    for (Index target_macro : neighbor_macros) {
                        // Topological gain: edges shared with target - edges shared with source
                        float topo_gain = 0.0f;
                        for (Index j = graph.xadj[micro]; j < graph.xadj[micro + 1]; ++j) {
                            Index neighbor = graph.adjncy[j];
                            Index neighbor_macro = assignment[neighbor];
                            
                            if (neighbor_macro == target_macro) {
                                topo_gain += graph.edgeW[j];
                            } else if (neighbor_macro == macro) {
                                topo_gain -= graph.edgeW[j];
                            }
                        }
                        
                        // Spatial gain: radius expansion penalty (using sphere volume)
                        Vector3f new_center = partition[target_macro].center;
                        float new_radius = partition[target_macro].radius;
                        merge_spheres(new_center, new_radius, graph.nodes[micro].center, graph.nodes[micro].radius);
                        
                        float old_volume = partition[target_macro].radius * partition[target_macro].radius * partition[target_macro].radius;
                        float new_volume = new_radius * new_radius * new_radius;
                        
                        float spatial_gain = -(new_volume - old_volume);
                        
                        // Combined gain
                        float total_gain = topo_weight * topo_gain + spatial_weight * spatial_gain;
                        
                        if (total_gain > best_gain) {
                            best_gain = total_gain;
                            best_target = target_macro;
                        }
                    }
                    
                    if (best_gain > 0.0f) {
                        boundary_heap.push({best_gain, micro});
                    }
                }
            }
        }
        
        // Apply moves until no improvement
        float cumulative_gain = 0.0f;
        float best_cumulative_gain = 0.0f;
        Index best_move_index = 0;
        
        while (!boundary_heap.empty()) {
            auto [gain, micro] = boundary_heap.top();
            boundary_heap.pop();
            
            if (locked[micro]) continue;
            
            Index source_macro = assignment[micro];
            Index target_macro = source_macro;
            float best_move_gain = gain;
            
            // Find best target again (gain may have changed)
            for (Index j = graph.xadj[micro]; j < graph.xadj[micro + 1]; ++j) {
                Index neighbor = graph.adjncy[j];
                Index neighbor_macro = assignment[neighbor];
                if (neighbor_macro != source_macro) {
                    // Recompute gain for this target
                    float topo_gain = 0.0f;
                    for (Index k = graph.xadj[micro]; k < graph.xadj[micro + 1]; ++k) {
                        Index n = graph.adjncy[k];
                        Index n_macro = assignment[n];
                        
                        if (n_macro == neighbor_macro) {
                            topo_gain += graph.edgeW[k];
                        } else if (n_macro == source_macro) {
                            topo_gain -= graph.edgeW[k];
                        }
                    }
                    
                    if (topo_gain > best_move_gain) {
                        best_move_gain = topo_gain;
                        target_macro = neighbor_macro;
                    }
                }
            }
            
            if (target_macro != source_macro && best_move_gain > 0.0f) {
                // Tentative move
                assignment[micro] = target_macro;
                locked[micro] = true;
                cumulative_gain += best_move_gain;
                move_log.push_back({best_move_gain, micro, target_macro});
                
                if (cumulative_gain > best_cumulative_gain) {
                    best_cumulative_gain = cumulative_gain;
                    best_move_index = move_log.size();
                }
            }
        }
        
        // Rollback to best state
        for (Index i = best_move_index; i < move_log.size(); ++i) {
            auto [gain, micro, target] = move_log[i];
            // Find source macro for this micro
            for (Index macro = 0; macro < num_macro; ++macro) {
                auto it = std::find(partition[macro].micro_nodes.begin(),
                                   partition[macro].micro_nodes.end(), micro);
                if (it != partition[macro].micro_nodes.end()) {
                    assignment[micro] = macro;
                    break;
                }
            }
        }
        
        if (best_cumulative_gain == 0.0f) break; // No improvement
    }
    
    // Rebuild partition cells
    for (Index macro = 0; macro < num_macro; ++macro) {
        partition[macro].micro_nodes.clear();
        partition[macro].triangle_count = 0;
        partition[macro].center = {0.0f, 0.0f, 0.0f};
        partition[macro].radius = 0.0f;
    }
    
    for (Index micro = 0; micro < graph.nodes.size(); ++micro) {
        Index macro = assignment[micro];
        partition[macro].micro_nodes.push_back(micro);
        partition[macro].triangle_count += graph.nodes[micro].triangle_count;
        if (partition[macro].radius == 0.0f) {
            partition[macro].center = graph.nodes[micro].center;
            partition[macro].radius = graph.nodes[micro].radius;
        } else {
            merge_spheres(partition[macro].center, partition[macro].radius,
                         graph.nodes[micro].center, graph.nodes[micro].radius);
        }
    }
}

} // namespace nx
