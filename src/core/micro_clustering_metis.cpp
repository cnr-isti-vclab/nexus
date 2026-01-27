#include "micro_clustering_metis.h"
#include "mesh.h"
#include "mesh_types.h"
#include <metis.h>
#include <vector>
#include <map>
#include <set>
#include <unordered_map>
#include <algorithm>
#include <limits>
#include <cmath>
#include <iostream>
#include <cassert>
#include <queue>
#include <fstream>
#include <filesystem>

#include <lemon/list_graph.h>
#include <lemon/matching.h>


using namespace std;
namespace nx {

namespace {

void export_graph_ply(const std::filesystem::path& path,
					  const std::vector<Vector3f>& vertices,
					  const std::vector<std::pair<Index, Index>>& edges) {
	std::ofstream out(path);
	if (!out) {
		std::cerr << "Failed to open " << path << " for writing" << std::endl;
		return;
	}

	out << "ply\n";
	out << "format ascii 1.0\n";
	out << "element vertex " << (vertices.size() + edges.size()) << "\n";
	out << "property float x\n";
	out << "property float y\n";
	out << "property float z\n";
	out << "element face " << edges.size() << "\n";
	out << "property list uchar int vertex_indices\n";
	out << "end_header\n";

	for (const Vector3f& v : vertices) {
		out << v.x << " " << v.y << " " << v.z << "\n";
	}

	const float eps = 1e-4f;
	std::vector<Vector3f> extra_vertices;
	extra_vertices.reserve(edges.size());
	for (std::size_t i = 0; i < edges.size(); ++i) {
		const auto& edge = edges[i];
		Index b = edge.second;
		const Vector3f& vb = vertices[b];
		Vector3f v2 = vb;
		float sx = ((i * 73856093u) & 1u) ? 1.0f : -1.0f;
		float sy = ((i * 19349663u) & 1u) ? 1.0f : -1.0f;
		float sz = ((i * 83492791u) & 1u) ? 1.0f : -1.0f;
		v2.x += eps * sx;
		v2.y += eps * sy;
		v2.z += eps * sz;
		extra_vertices.push_back(v2);
	}
	for (const Vector3f& v : extra_vertices) {
		out << v.x << " " << v.y << " " << v.z << "\n";
	}

	for (std::size_t i = 0; i < edges.size(); ++i) {
		const auto& edge = edges[i];
		Index a = edge.first;
		Index b = edge.second;
		Index v2_index = static_cast<Index>(vertices.size() + i);
		out << "3 " << a << " " << b << " " << v2_index << "\n";
	}
}

std::vector<std::pair<Index, Index>> build_undirected_edges_from_csr(
    const std::vector<idx_t>& xadj,
    const std::vector<idx_t>& adjncy) {

    std::vector<std::pair<Index, Index>> edges;
    std::size_t num_nodes = (xadj.size() > 0) ? (xadj.size() - 1) : 0;
    for (std::size_t i = 0; i < num_nodes; ++i) {
        idx_t start = xadj[i];
        idx_t end = xadj[i + 1];
        for (idx_t e = start; e < end; ++e) {
            Index j = static_cast<Index>(adjncy[e]);
            if (j == static_cast<Index>(i)) continue;
            if (static_cast<Index>(i) < j) {
                edges.emplace_back(static_cast<Index>(i), j);
            }
        }
    }
    return edges;
}


// Maximum weight matching using LEMON library
// Returns pairs of matched node indices
std::vector<std::pair<std::size_t, std::size_t>> max_weight_matching(
    std::size_t num_nodes,
    const std::vector<idx_t>& xadj,
    const std::vector<idx_t>& adjncy,
    const std::vector<idx_t>& adjwgt) {

    using namespace lemon;

    // Create LEMON graph
    ListGraph graph;
    ListGraph::EdgeMap<idx_t> weight(graph);

    // Add nodes
    std::vector<ListGraph::Node> nodes(num_nodes);
    for (std::size_t i = 0; i < num_nodes; ++i) {
        nodes[i] = graph.addNode();
    }

    // Add edges with weights
    for (std::size_t i = 0; i < num_nodes; ++i) {
        idx_t start = xadj[i];
        idx_t end = xadj[i + 1];
        for (idx_t e = start; e < end; ++e) {
            std::size_t j = static_cast<std::size_t>(adjncy[e]);
            if (i < j) { // Add each edge once
                ListGraph::Edge edge = graph.addEdge(nodes[i], nodes[j]);
                weight[edge] = adjwgt[e];
            }
        }
    }

    // Run maximum weight matching
    MaxWeightedMatching<ListGraph, ListGraph::EdgeMap<idx_t>> matching(graph, weight);
    matching.run();

    // Extract matched pairs
    std::vector<std::pair<std::size_t, std::size_t>> matched_pairs;
    for (std::size_t i = 0; i < num_nodes; ++i) {
        ListGraph::Node mate_node = matching.mate(nodes[i]);
        if (mate_node != INVALID) {
            // Find the index of the mate node
            for (std::size_t j = i + 1; j < num_nodes; ++j) {
                if (nodes[j] == mate_node) {
                    matched_pairs.emplace_back(i, j);
                    break;
                }
            }
        }
    }

    return matched_pairs;
}

// Partition graph into groups of exactly target_size using two rounds of matching
// For 4-way partition: first round pairs nodes into groups of 2, second round pairs groups into groups of 4
std::vector<idx_t> partition_graph_exact_size(
    std::size_t num_nodes,
    const std::vector<idx_t>& xadj,
    const std::vector<idx_t>& adjncy,
    const std::vector<idx_t>& adjwgt,
    std::size_t target_size,
    std::size_t num_parts) {

    std::vector<idx_t> part(num_nodes, idx_t(-1));
    if (num_nodes == 0 || num_parts == 0) return part;

    // For target_size == 4, we do two rounds of matching
    // Round 1: pair nodes into groups of 2
    // Round 2: pair groups into groups of 4

    if (target_size == 4 && num_nodes >= 4) {
        std::cout << "Using two-round matching for exact 4-way partition..." << std::endl;

        // Round 1: Maximum weight matching to pair nodes (using Hungarian algorithm)
        auto pairs_round1 = max_weight_matching(num_nodes, xadj, adjncy, adjwgt);

        // Handle unmatched nodes (odd count or disconnected)
        std::vector<char> matched_r1(num_nodes, 0);
        for (const auto& [u, v] : pairs_round1) {
            matched_r1[u] = 1;
            matched_r1[v] = 1;
        }

        // Collect unmatched nodes
        std::vector<std::size_t> unmatched;
        for (std::size_t i = 0; i < num_nodes; ++i) {
            if (!matched_r1[i]) {
                unmatched.push_back(i);
            }
        }

        // Pair unmatched nodes arbitrarily
        for (std::size_t i = 0; i + 1 < unmatched.size(); i += 2) {
            pairs_round1.emplace_back(unmatched[i], unmatched[i + 1]);
        }

        // If odd number of nodes, last one forms a singleton "pair"
        std::vector<std::vector<std::size_t>> groups_r1;
        groups_r1.reserve(pairs_round1.size() + 1);
        for (const auto& [u, v] : pairs_round1) {
            groups_r1.push_back({u, v});
        }
        if (unmatched.size() % 2 == 1) {
            groups_r1.push_back({unmatched.back()});
        }

        std::cout << "Round 1: Created " << groups_r1.size() << " pairs from " << num_nodes << " nodes" << std::endl;

        // Build super-graph where each node is a group from round 1
        // Edge weight between super-nodes = sum of edge weights between their constituent nodes
        std::size_t num_super_nodes = groups_r1.size();

        // Map original node -> super-node
        std::vector<std::size_t> node_to_super(num_nodes);
        for (std::size_t g = 0; g < groups_r1.size(); ++g) {
            for (std::size_t v : groups_r1[g]) {
                node_to_super[v] = g;
            }
        }

        // Build super-graph adjacency
        std::vector<std::map<std::size_t, idx_t>> super_adj(num_super_nodes);
        for (std::size_t i = 0; i < num_nodes; ++i) {
            std::size_t si = node_to_super[i];
            idx_t start = xadj[i];
            idx_t end = xadj[i + 1];
            for (idx_t e = start; e < end; ++e) {
                std::size_t j = static_cast<std::size_t>(adjncy[e]);
                std::size_t sj = node_to_super[j];
                if (si != sj) {
                    super_adj[si][sj] += adjwgt[e];
                }
            }
        }

        // Convert super-graph to CSR
        std::vector<idx_t> super_xadj, super_adjncy, super_adjwgt;
        super_xadj.reserve(num_super_nodes + 1);
        super_xadj.push_back(0);
        for (std::size_t i = 0; i < num_super_nodes; ++i) {
            for (const auto& [neighbor, weight] : super_adj[i]) {
                super_adjncy.push_back(static_cast<idx_t>(neighbor));
                super_adjwgt.push_back(weight);
            }
            super_xadj.push_back(static_cast<idx_t>(super_adjncy.size()));
        }

        // Round 2: Maximum weight matching on super-graph (using Hungarian algorithm)
        auto pairs_round2 = max_weight_matching(num_super_nodes, super_xadj, super_adjncy, super_adjwgt);

        // Handle unmatched super-nodes
        std::vector<char> matched_r2(num_super_nodes, 0);
        for (const auto& [u, v] : pairs_round2) {
            matched_r2[u] = 1;
            matched_r2[v] = 1;
        }

        std::vector<std::size_t> unmatched_super;
        for (std::size_t i = 0; i < num_super_nodes; ++i) {
            if (!matched_r2[i]) {
                unmatched_super.push_back(i);
            }
        }

        // Pair unmatched super-nodes arbitrarily
        for (std::size_t i = 0; i + 1 < unmatched_super.size(); i += 2) {
            pairs_round2.emplace_back(unmatched_super[i], unmatched_super[i + 1]);
        }

        std::cout << "Round 2: Created " << pairs_round2.size() << " super-pairs" << std::endl;

        // Build final groups of 4 (or less for remainders)
        std::vector<std::vector<std::size_t>> final_groups;
        final_groups.reserve(pairs_round2.size() + 1);

        for (const auto& [su, sv] : pairs_round2) {
            std::vector<std::size_t> group;
            for (std::size_t v : groups_r1[su]) group.push_back(v);
            for (std::size_t v : groups_r1[sv]) group.push_back(v);
            final_groups.push_back(std::move(group));
        }

        // Handle remaining unmatched super-nodes (singleton super-groups)
        if (unmatched_super.size() % 2 == 1) {
            std::size_t last_super = unmatched_super.back();
            final_groups.push_back(groups_r1[last_super]);
        }

        std::cout << "Final: " << final_groups.size() << " groups" << std::endl;

        // Assign partition IDs
        for (std::size_t p = 0; p < final_groups.size(); ++p) {
            for (std::size_t v : final_groups[p]) {
                part[v] = static_cast<idx_t>(p);
            }
        }

        // Verify all nodes assigned
        for (std::size_t i = 0; i < num_nodes; ++i) {
			if (part[i] == idx_t(-1)) {
				throw std::runtime_error("Unassigned part");
            }
        }

        return part;
    }

    // Fallback for non-4 target sizes: simple sequential assignment
    std::cout << "Fallback: sequential assignment for target_size != 4" << std::endl;
    for (std::size_t i = 0; i < num_nodes; ++i) {
        part[i] = static_cast<idx_t>(i / target_size);
        if (part[i] >= static_cast<idx_t>(num_parts)) {
            part[i] = static_cast<idx_t>(num_parts - 1);
        }
    }
    return part;
}
// Convert adjacency map to CSR format
void adjacency_to_csr(const std::vector<std::map<Index, int>>& adjacency,
					  std::vector<idx_t>& xadj,
					  std::vector<idx_t>& adjncy,
					  std::vector<idx_t>& adjwgt) {
	xadj.clear();
	adjncy.clear();
	adjwgt.clear();

	std::size_t num_nodes = adjacency.size();
	xadj.reserve(num_nodes + 1);
	xadj.push_back(0);

	for (std::size_t i = 0; i < num_nodes; ++i) {
		for (const auto& [neighbor, weight] : adjacency[i]) {
			adjncy.push_back(static_cast<idx_t>(neighbor));
			adjwgt.push_back(static_cast<idx_t>(weight));
		}
		xadj.push_back(static_cast<idx_t>(adjncy.size()));
	}
}

// Build cluster adjacency graph with edge weights = number of shared triangle edges
void build_cluster_graph_weighted(const MeshFiles& mesh,
								  std::vector<idx_t>& xadj,
								  std::vector<idx_t>& adjncy,
								  std::vector<idx_t>& adjwgt) {
	std::size_t num_clusters = mesh.clusters.size();

	// Build cluster-to-micronode mapping if removing internal edges
	std::vector<Index> cluster_to_micronode;

	// Use map to accumulate edge weights (undirected graph, store each edge once per direction)
	std::vector<std::map<Index, int>> adjacency(num_clusters);

	// Build adjacency by checking triangle neighbors
	for (std::size_t tri_idx = 0; tri_idx < mesh.triangle_to_cluster.size(); ++tri_idx) {
		Index cluster_id = mesh.triangle_to_cluster[tri_idx];
		const FaceAdjacency& adj = mesh.adjacency[tri_idx];

		for (int corner = 0; corner < 3; ++corner) {
			Index neighbor_tri = adj.opp[corner];
			if (neighbor_tri != std::numeric_limits<Index>::max()) {
				Index neighbor_cluster = mesh.triangle_to_cluster[neighbor_tri];
				if (neighbor_cluster != cluster_id) {
					// Accumulate edge count (each shared triangle edge increments)
					adjacency[cluster_id][neighbor_cluster]++;
				}
			}
		}
	}

	// Convert to CSR format
	adjacency_to_csr(adjacency, xadj, adjncy, adjwgt);
}

// Build micronode adjacency graph (CSR) from a cluster adjacency graph (CSR).
// If primary_parent_of_child is provided, edges connecting child micronodes within the same
// primary parent are removed (used for dual partitioning).
void build_micronode_graph_from_cluster_graph(
	const MeshFiles& mesh,
	const std::vector<idx_t>& cluster_xadj,
	const std::vector<idx_t>& cluster_adjncy,
	const std::vector<idx_t>& cluster_adjwgt,
	std::vector<idx_t>& micronode_xadj,
	std::vector<idx_t>& micronode_adjncy,
	std::vector<idx_t>& micronode_adjwgt,
	const std::vector<Index>* primary_parent_of_child = nullptr) {

	std::size_t num_micronodes = mesh.micronodes.size();
	if (num_micronodes == 0) {
		micronode_xadj.clear();
		micronode_adjncy.clear();
		micronode_adjwgt.clear();
		return;
	}

	// Build cluster -> child micronode mapping
	std::vector<Index> cluster_to_micronode(mesh.clusters.size(), Index(-1));
	for (std::size_t mn_id = 0; mn_id < mesh.micronodes.size(); ++mn_id) {
		for (Index cluster_id : mesh.micronodes[mn_id].cluster_ids) {
			cluster_to_micronode[cluster_id] = static_cast<Index>(mn_id);
		}
	}

	// Accumulate micronode adjacency by summing cluster-edge weights
	std::vector<std::map<Index, int>> adjacency(num_micronodes);
	std::size_t num_clusters = mesh.clusters.size();
	for (std::size_t c = 0; c < num_clusters; ++c) {
		Index mn_c = cluster_to_micronode[c];
		if (mn_c == Index(-1)) {
			continue;
		}
		idx_t start = cluster_xadj[c];
		idx_t end = cluster_xadj[c + 1];
		for (idx_t e = start; e < end; ++e) {
			Index neighbor_cluster = static_cast<Index>(cluster_adjncy[e]);
			Index mn_n = cluster_to_micronode[neighbor_cluster];
			if (mn_n == Index(-1) || mn_n == mn_c) {
				continue;
			}
			if (primary_parent_of_child) {
				if ((*primary_parent_of_child)[mn_c] == (*primary_parent_of_child)[mn_n]) {
					continue;
				}
			}
			int w = static_cast<int>(cluster_adjwgt[e]);
			adjacency[mn_c][mn_n] += w;
		}
	}

	adjacency_to_csr(adjacency, micronode_xadj, micronode_adjncy, micronode_adjwgt);
}

// Build parent micronodes from partition of child micronodes
std::vector<MicroNode> build_parent_micronodes_from_partition(
	const MeshFiles& mesh,
	const std::vector<idx_t>& part,
	std::size_t num_parts) {

	std::size_t num_children = mesh.micronodes.size();
	std::vector<std::vector<Index>> parent_children(num_parts);
	for (std::size_t i = 0; i < num_children; ++i) {
		Index parent_id = static_cast<Index>(part[i]);
		parent_children[parent_id].push_back(static_cast<Index>(i));
	}

	std::vector<MicroNode> parents;
	parents.reserve(num_parts);

	for (std::size_t i = 0; i < num_parts; ++i) {
		if (parent_children[i].empty()) {
			throw std::runtime_error("Empty parent micronode!");
		}

		MicroNode parent;
		parent.id = static_cast<Index>(i);
		parent.children_nodes = std::move(parent_children[i]);
		parent.triangle_count = 0;
		parent.centroid = {0.0f, 0.0f, 0.0f};
		float total_weight = 0.0f;

		for (Index child_id : parent.children_nodes) {
			const MicroNode& child = mesh.micronodes[child_id];
			float weight = static_cast<float>(child.triangle_count);
			parent.centroid.x += child.centroid.x * weight;
			parent.centroid.y += child.centroid.y * weight;
			parent.centroid.z += child.centroid.z * weight;
			total_weight += weight;
			parent.triangle_count += child.triangle_count;
		}

		if (total_weight > 0.0f) {
			parent.centroid.x /= total_weight;
			parent.centroid.y /= total_weight;
			parent.centroid.z /= total_weight;
		} else {
			float inv_count = 1.0f / static_cast<float>(parent.children_nodes.size());
			for (Index child_id : parent.children_nodes) {
				const MicroNode& child = mesh.micronodes[child_id];
				parent.centroid.x += child.centroid.x * inv_count;
				parent.centroid.y += child.centroid.y * inv_count;
				parent.centroid.z += child.centroid.z * inv_count;
			}
		}

		parent.center = parent.centroid;
		parent.radius = 0.0f;
		for (Index child_id : parent.children_nodes) {
			const MicroNode& child = mesh.micronodes[child_id];
			float dx = child.center.x - parent.center.x;
			float dy = child.center.y - parent.center.y;
			float dz = child.center.z - parent.center.z;
			float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
			parent.radius = std::max(parent.radius, dist + child.radius);
		}

		parents.push_back(std::move(parent));
	}

	return parents;
}

// Build MicroNode objects from partition and mesh
std::vector<MicroNode> build_micronodes_from_partition(const MeshFiles& mesh,
													   const std::vector<idx_t>& part,
													   std::size_t num_parts) {
	std::size_t num_clusters = mesh.clusters.size();

	// Group clusters by partition
	std::vector<std::vector<Index>> micronode_clusters(num_parts);
	for (std::size_t i = 0; i < num_clusters; ++i) {
		Index micronode_id = static_cast<Index>(part[i]);
		micronode_clusters[micronode_id].push_back(static_cast<Index>(i));
	}

	// Create MicroNode objects
	std::vector<MicroNode> micronodes;
	micronodes.reserve(num_parts);

	for (std::size_t i = 0; i < num_parts; ++i) {
		if (micronode_clusters[i].empty())
			throw std::runtime_error("Empty micronode clusters!");

		MicroNode micronode;
		micronode.id = static_cast<Index>(i);
		micronode.cluster_ids = std::move(micronode_clusters[i]);
		micronode.triangle_count = 0;

		// Compute weighted centroid
		micronode.centroid = {0.0f, 0.0f, 0.0f};
		float total_weight = 0.0f;

		for (Index cluster_id : micronode.cluster_ids) {
			const Cluster& c = mesh.clusters[cluster_id];
			float weight = static_cast<float>(c.triangle_count);
			micronode.centroid.x += c.center.x * weight;
			micronode.centroid.y += c.center.y * weight;
			micronode.centroid.z += c.center.z * weight;
			total_weight += weight;
			micronode.triangle_count += c.triangle_count;
		}

		if (total_weight > 0.0f) {
			micronode.centroid.x /= total_weight;
			micronode.centroid.y /= total_weight;
			micronode.centroid.z /= total_weight;
		}

		// Compute bounding sphere
		micronode.center = micronode.centroid;
		micronode.radius = 0.0f;

		for (Index cluster_id : micronode.cluster_ids) {
			const Cluster& c = mesh.clusters[cluster_id];
			float dx = c.center.x - micronode.center.x;
			float dy = c.center.y - micronode.center.y;
			float dz = c.center.z - micronode.center.z;
			float dist = std::sqrt(dx*dx + dy*dy + dz*dz);
			float extent = dist + c.radius;
			micronode.radius = std::max(micronode.radius, extent);
		}

		micronodes.push_back(std::move(micronode));
	}

	return micronodes;
}

// Call METIS to partition graph into specified number of parts
// Returns partition assignment for each node, and sets objval to edge-cut
std::vector<idx_t> partition_graph_metis(
	std::size_t num_nodes,
	const std::vector<idx_t>& xadj,
	const std::vector<idx_t>& adjncy,
	const std::vector<idx_t>& adjwgt,
	std::size_t num_partitions,
	idx_t& objval) {

	idx_t nvtxs = static_cast<idx_t>(num_nodes);
	idx_t ncon = 1;
	idx_t nparts = static_cast<idx_t>(num_partitions);

	std::vector<idx_t> part(num_nodes);
	std::vector<idx_t> vwgt(num_nodes, 1);
	std::vector<real_t> tpwgts(num_partitions, 0.0);
	if (num_partitions > 0) {
		real_t w = static_cast<real_t>(1.0) / static_cast<real_t>(num_partitions);
		for (std::size_t i = 0; i < num_partitions; ++i) tpwgts[i] = w;
	}

	idx_t options[METIS_NOPTIONS];
	METIS_SetDefaultOptions(options);
	options[METIS_OPTION_OBJTYPE] = METIS_OBJTYPE_CUT;
	options[METIS_OPTION_NUMBERING] = 0;
	options[METIS_OPTION_UFACTOR] = 1; // balance tolerance

	int ret = METIS_PartGraphKway(
		&nvtxs, &ncon,
		const_cast<idx_t*>(xadj.data()),
		const_cast<idx_t*>(adjncy.data()),
		vwgt.data(), nullptr,
		const_cast<idx_t*>(adjwgt.data()),
		&nparts,
		tpwgts.data(), nullptr,
		options,
		&objval,
		part.data()
		);

	if (ret != METIS_OK) {
		throw std::runtime_error("METIS_PartGraphKway failed with code " + std::to_string(ret));
	}

	return part;
}

} // anonymous namespace
/*
DualPartitionSplit split_triangles_dual_partition(
	const MeshFiles& mesh,
	const std::vector<Index>& triangle_indices,
	const Vector3f& primary_centroid,
	const Vector3f& dual_centroid) {
	return split_triangles_dual_partition_impl(mesh, triangle_indices, primary_centroid, dual_centroid);
}*/

std::vector<MicroNode> create_micronodes_metis(const MeshFiles& mesh,
											   std::size_t clusters_per_micronode) {
	std::cout << "\n=== Building micronodes with METIS ===" << std::endl;

	std::size_t num_clusters = mesh.clusters.size();
	if (num_clusters == 0) {
		std::cerr << "Error: No clusters to partition into micronodes" << std::endl;
		return {};
	}

	// Estimate number of micronodes
	std::size_t num_micronodes = (num_clusters + clusters_per_micronode - 1) / clusters_per_micronode;
	if (num_micronodes < 2) num_micronodes = 2; // METIS requires at least 2 partitions

	std::cout << "Clusters: " << num_clusters << ", Target per micronode: " << clusters_per_micronode
			  << ", Estimated micronodes: " << num_micronodes << std::endl;

	// Build weighted cluster graph
	std::vector<idx_t> xadj;
	std::vector<idx_t> adjncy;
	std::vector<idx_t> adjwgt;

	build_cluster_graph_weighted(mesh, xadj, adjncy, adjwgt);

	std::cout << "Cluster graph edges: " << adjncy.size()
			  << " (weighted by shared triangle edges)" << std::endl;

	if (adjncy.empty()) {
		std::cerr << "Error: Cluster graph has no edges (disconnected clusters)" << std::endl;
		return {};
	}

	// Exact-size partitioning (prioritize clusters_per_micronode over cut)
	std::cout << "Calling exact-size greedy partitioner with target size " << clusters_per_micronode
			  << "..." << std::endl;

	idx_t objval = 0;
	std::vector<idx_t> part = partition_graph_exact_size(num_clusters, xadj, adjncy, adjwgt,
									   clusters_per_micronode, num_micronodes);

	if (part.empty()) {
		throw std::runtime_error("Exact-size partitioning failed!");
	}
	//find the ACTUAL number of parts:
	num_micronodes = static_cast<std::size_t>(*std::max_element(part.begin(), part.end())) + 1;

	std::cout << "METIS partitioning complete. Edge-cut: " << objval << std::endl;

	// Build MicroNode structures from partition
	std::vector<MicroNode> micronodes = build_micronodes_from_partition(mesh, part, num_micronodes);

	// Report histogram
	std::cout << "\nMicronode histogram:" << std::endl;
	std::cout << "  Total micronodes: " << micronodes.size() << std::endl;

	if (micronodes.empty()) {
		throw std::runtime_error("No micronodes were created!");
	}
	std::size_t min_clusters = std::numeric_limits<std::size_t>::max();
	std::size_t max_clusters = 0;
	for (const MicroNode& mn : micronodes) {
		std::size_t n = mn.cluster_ids.size();
		min_clusters = std::min(min_clusters, n);
		max_clusters = std::max(max_clusters, n);
	}
	if (min_clusters == std::numeric_limits<std::size_t>::max()) min_clusters = 0;

	std::vector<std::size_t> hist(max_clusters + 1, 0);
	for (const MicroNode& mn : micronodes) {
		std::size_t n = mn.cluster_ids.size();
		if (n >= hist.size()) hist.resize(n + 1, 0);
		hist[n]++;
	}

	std::size_t max_freq = 0;
	for (std::size_t count : hist) max_freq = std::max(max_freq, count);

	for (std::size_t n = 0; n < hist.size(); ++n) {
		std::size_t freq = hist[n];
		if (freq == 0) continue;
		std::size_t bars = (max_freq > 0) ? (freq * 50 / max_freq) : 0;
		std::cout << "  " << n << ": " << freq << " ";
		for (std::size_t i = 0; i < bars; ++i) std::cout << '#';
		std::cout << std::endl;
	}
	std::cout << "\nMETIS micronode clustering complete" << std::endl;

	return micronodes;
}


void create_parent_micronodes(MeshFiles& mesh, MeshFiles &parent,
							  std::size_t clusters_per_micronode ) {

	std::size_t num_child_micronodes = mesh.micronodes.size();
	if (num_child_micronodes == 0) {
		throw std::runtime_error("Error: No child micronodes to partition");
	}

	// Calculate number of parent micronodes (micronodes per micronode)
	std::size_t num_parent_micronodes = (num_child_micronodes + clusters_per_micronode - 1) / clusters_per_micronode;
	if (num_parent_micronodes == 0) num_parent_micronodes = 1;

	std::cout << "Target parent micronodes: " << num_parent_micronodes << std::endl;

	// Step 1: Build weighted cluster graph
	std::vector<idx_t> cluster_xadj, cluster_adjncy, cluster_adjwgt;
	build_cluster_graph_weighted(mesh, cluster_xadj, cluster_adjncy, cluster_adjwgt);
	{
		std::vector<Vector3f> cluster_vertices;
		cluster_vertices.reserve(mesh.clusters.size());
		for(size_t i = 0; i < mesh.clusters.size(); i++) {
			Cluster &c = mesh.clusters[i];
			cluster_vertices.push_back(c.center);
		}
		auto edges = build_undirected_edges_from_csr(cluster_xadj, cluster_adjncy);
		const std::filesystem::path out_dir = std::filesystem::current_path();
		const std::string tag = "clusters_" + std::to_string(mesh.clusters.size());
		export_graph_ply(out_dir / (tag + "_graph.ply"), cluster_vertices, edges);
	}

	if (cluster_adjncy.empty()) {
		throw std::runtime_error("Error: Cluster graph has no edges");
	}

	// Step 2: Build micronode graph from cluster graph
	std::vector<idx_t> micronode_xadj, micronode_adjncy, micronode_adjwgt;
	build_micronode_graph_from_cluster_graph(mesh, cluster_xadj, cluster_adjncy, cluster_adjwgt,
									  micronode_xadj, micronode_adjncy, micronode_adjwgt, nullptr);
	{
		std::vector<Vector3f> micronode_vertices;
		micronode_vertices.reserve(mesh.micronodes.size());
		for (const MicroNode& mn : mesh.micronodes) {
			micronode_vertices.push_back(mn.centroid);
		}
		auto edges = build_undirected_edges_from_csr(micronode_xadj, micronode_adjncy);
		const std::filesystem::path out_dir = std::filesystem::current_path();
		const std::string tag = "micronodes_" + std::to_string(mesh.micronodes.size());
		export_graph_ply(out_dir / (tag + "_graph.ply"), micronode_vertices, edges);
	}

	if (micronode_adjncy.empty()) {
		throw std::runtime_error("Error: Micronode graph has no edges");
	}

	// Exact-size partitioning for primary parent partition (micronode graph)
	std::cout << "Calling exact-size partitioner for primary parent partition..." << std::endl;
	std::vector<idx_t> part_primary = partition_graph_exact_size(
		num_child_micronodes, micronode_xadj, micronode_adjncy, micronode_adjwgt,
		clusters_per_micronode, num_parent_micronodes);

	num_parent_micronodes = static_cast<std::size_t>(*std::max_element(part_primary.begin(), part_primary.end())) + 1;

	// Build primary parent micronodes (children_nodes + centroids)
	std::vector<MicroNode> primary_micronodes =
		build_parent_micronodes_from_partition(mesh, part_primary, num_parent_micronodes);

	std::cout << "Created " << primary_micronodes.size() << " primary parent micronodes" << std::endl;

	// Step 3: Build dual parent micronodes using micronode graph with internal edges removed
	std::cout << "\n--- Step 2: Dual Parent Micronodes ---" << std::endl;

	std::vector<Index> child_to_primary_parent(num_child_micronodes, Index(-1));
	for (std::size_t i = 0; i < num_child_micronodes; ++i) {
		child_to_primary_parent[i] = static_cast<Index>(part_primary[i]);
	}

	std::vector<idx_t> micronode_xadj_ext, micronode_adjncy_ext, micronode_adjwgt_ext;
	build_micronode_graph_from_cluster_graph(mesh, cluster_xadj, cluster_adjncy, cluster_adjwgt,
									  micronode_xadj_ext, micronode_adjncy_ext, micronode_adjwgt_ext,
									  &child_to_primary_parent);

	if (micronode_adjncy_ext.empty()) {
		throw std::runtime_error("Error: Micronode graph has no edges after removing internal edges");
	}

	std::cout << "External micronode edges: " << micronode_adjncy_ext.size() << std::endl;

	// Exact-size partitioning for dual parent partition (micronode graph)
	std::cout << "Calling exact-size partitioner for dual parent partition..." << std::endl;
	std::vector<idx_t> part_dual = partition_graph_exact_size(
		num_child_micronodes, micronode_xadj_ext, micronode_adjncy_ext, micronode_adjwgt_ext,
		clusters_per_micronode, num_parent_micronodes);

	std::size_t num_dual_micronodes = static_cast<std::size_t>(*std::max_element(part_dual.begin(), part_dual.end())) + 1;


	// Build dual parent micronodes (children_nodes + centroids)
	std::vector<MicroNode> dual_micronodes =
		build_parent_micronodes_from_partition(mesh, part_dual, num_dual_micronodes);

	std::cout << "Created " << dual_micronodes.size() << " dual parent micronodes" << std::endl;

	std::cout << "\n=== Primary and Dual Parent Micronodes Complete ===" << std::endl;

	// Create micronodes in parent mesh from primary + dual partitions
	parent.micronodes.reserve(primary_micronodes.size() + dual_micronodes.size());

	for (const MicroNode& micronode : primary_micronodes) {
		parent.micronodes.push_back(micronode);
	}

	for (const MicroNode& micronode : dual_micronodes) {
		parent.micronodes.push_back(micronode);
	}
}

float measure_micronode_overlap(const MeshFiles& mesh,
								const std::vector<MicroNode>& primary_micronodes,
								const std::vector<MicroNode>& dual_micronodes) {

	std::cout << "\n=== Measuring Primary-Dual Micronode Overlap ===" << std::endl;

	// For each primary micronode, find max cluster overlap with any dual micronode
	int count_with_3_plus = 0;
	std::map<int, int> overlap_histogram;

	for (std::size_t p = 0; p < primary_micronodes.size(); ++p) {
		const auto& primary_clusters = primary_micronodes[p].cluster_ids;
		std::set<Index> primary_set(primary_clusters.begin(), primary_clusters.end());

		int max_shared_clusters = 0;

		// Check overlap with each dual micronode
		for (std::size_t d = 0; d < dual_micronodes.size(); ++d) {
			const auto& dual_clusters = dual_micronodes[d].cluster_ids;

			// Count intersection (shared clusters)
			int shared = 0;
			for (Index cluster_id : dual_clusters) {
				if (primary_set.count(cluster_id) > 0) {
					shared++;
				}
			}

			max_shared_clusters = std::max(max_shared_clusters, shared);
		}

		overlap_histogram[max_shared_clusters]++;

		if (max_shared_clusters >= 3) {
			count_with_3_plus++;
		}
	}

	float fraction = static_cast<float>(count_with_3_plus) / primary_micronodes.size();

	std::cout << "Primary micronodes with 3+ shared clusters with a dual micronode: "
			  << count_with_3_plus << " / " << primary_micronodes.size()
			  << " (" << (fraction * 100.0f) << "%)" << std::endl;

	// Print distribution
	std::cout << "\nShared cluster distribution:" << std::endl;
	for (const auto& [shared_count, num_micronodes] : overlap_histogram) {
		std::cout << "  " << shared_count << " shared clusters: "
				  << num_micronodes << " primary micronodes" << std::endl;
	}

	return fraction;
}

void create_parent_micronodes2(MeshFiles& mesh, MeshFiles& parent,
							   std::size_t /*clusters_per_micronode*/) {
	std::cout << "\n=== Building parent micronodes using triangle graph matching ===" << std::endl;

	std::size_t num_child_micronodes = mesh.micronodes.size();
	if (num_child_micronodes == 0) {
		throw std::runtime_error("Error: No child micronodes to partition");
	}

	// Step 1: Build weighted cluster graph
	std::vector<idx_t> cluster_xadj, cluster_adjncy, cluster_adjwgt;
	build_cluster_graph_weighted(mesh, cluster_xadj, cluster_adjncy, cluster_adjwgt);

	if (cluster_adjncy.empty()) {
		throw std::runtime_error("Error: Cluster graph has no edges");
	}

	// Step 2: Build micronode graph from cluster graph
	std::vector<idx_t> micronode_xadj, micronode_adjncy, micronode_adjwgt;
	build_micronode_graph_from_cluster_graph(mesh, cluster_xadj, cluster_adjncy, cluster_adjwgt,
									  micronode_xadj, micronode_adjncy, micronode_adjwgt, nullptr);

	if (micronode_adjncy.empty()) {
		throw std::runtime_error("Error: Micronode graph has no edges");
	}

	// Build adjacency set for quick neighbor lookup
	std::vector<std::set<std::size_t>> adj_set(num_child_micronodes);
	std::map<std::pair<std::size_t, std::size_t>, idx_t> edge_weight;
	for (std::size_t i = 0; i < num_child_micronodes; ++i) {
		idx_t start = micronode_xadj[i];
		idx_t end = micronode_xadj[i + 1];
		for (idx_t e = start; e < end; ++e) {
			std::size_t j = static_cast<std::size_t>(micronode_adjncy[e]);
			adj_set[i].insert(j);
			if (i < j) {
				edge_weight[{i, j}] = micronode_adjwgt[e];
			}
		}
	}

	// Step 3: Find all triangles in the micronode graph
	// A triangle is (a, b, c) with a < b < c and edges a-b, b-c, a-c all exist
	struct Triangle {
		std::size_t a, b, c;
		// Store which edges this triangle has (for adjacency lookup)
		std::array<std::pair<std::size_t, std::size_t>, 3> edges() const {
			return {{{a, b}, {a, c}, {b, c}}};
		}
	};
	std::vector<Triangle> triangles;

	for (std::size_t a = 0; a < num_child_micronodes; ++a) {
		for (std::size_t b : adj_set[a]) {
			if (b <= a) continue;
			for (std::size_t c : adj_set[b]) {
				if (c <= b) continue;
				// Check if a-c edge exists (completes the triangle)
				if (adj_set[a].count(c)) {
					triangles.push_back({a, b, c});
				}
			}
		}
	}

	std::cout << "Found " << triangles.size() << " triangles in micronode graph" << std::endl;

	if (triangles.empty()) {
		throw std::runtime_error("Error: No triangles found in micronode graph");
	}

	// Step 4: Build edge-to-triangles mapping
	// For each edge, store which triangles contain it
	std::map<std::pair<std::size_t, std::size_t>, std::vector<std::size_t>> edge_to_triangles;
	for (std::size_t t = 0; t < triangles.size(); ++t) {
		for (const auto& edge : triangles[t].edges()) {
			edge_to_triangles[edge].push_back(t);
		}
	}

	// Step 5: Build triangle adjacency graph
	// Two triangles are adjacent if they share an edge
	// Edge weight = weight of the shared edge in the micronode graph
	std::size_t num_triangle_nodes = triangles.size();
	std::vector<std::map<std::size_t, idx_t>> triangle_adjacency(num_triangle_nodes);

	for (const auto& [edge, tri_list] : edge_to_triangles) {
		idx_t w = edge_weight[edge];
		// All pairs of triangles sharing this edge are adjacent
		for (std::size_t i = 0; i < tri_list.size(); ++i) {
			for (std::size_t j = i + 1; j < tri_list.size(); ++j) {
				std::size_t t1 = tri_list[i];
				std::size_t t2 = tri_list[j];
				triangle_adjacency[t1][t2] += w;
				triangle_adjacency[t2][t1] += w;
			}
		}
	}

	// Convert triangle adjacency to CSR
	std::vector<idx_t> tri_xadj, tri_adjncy, tri_adjwgt;
	tri_xadj.reserve(num_triangle_nodes + 1);
	tri_xadj.push_back(0);
	for (std::size_t i = 0; i < num_triangle_nodes; ++i) {
		for (const auto& [neighbor, weight] : triangle_adjacency[i]) {
			tri_adjncy.push_back(static_cast<idx_t>(neighbor));
			tri_adjwgt.push_back(weight);
		}
		tri_xadj.push_back(static_cast<idx_t>(tri_adjncy.size()));
	}

	std::cout << "Triangle graph: " << num_triangle_nodes << " nodes, "
			  << tri_adjncy.size() / 2 << " edges" << std::endl;

	// Step 6: Apply maximum weight matching on triangle graph
	auto matched_triangle_pairs = max_weight_matching(num_triangle_nodes, tri_xadj, tri_adjncy, tri_adjwgt);

	std::cout << "Matched " << matched_triangle_pairs.size() << " triangle pairs" << std::endl;

	// Step 7: Build parent micronodes from matched triangle pairs
	// Each matched pair = 2 triangles sharing an edge = 4 unique vertices (micronodes)
	std::vector<char> triangle_used(num_triangle_nodes, 0);
	std::vector<MicroNode> parent_micronodes;

	for (const auto& [t1, t2] : matched_triangle_pairs) {
		triangle_used[t1] = 1;
		triangle_used[t2] = 1;

		const Triangle& tri1 = triangles[t1];
		const Triangle& tri2 = triangles[t2];

		// Collect the unique vertices (should be 4 if they share exactly one edge)
		std::set<std::size_t> vertices_set;
		vertices_set.insert(tri1.a);
		vertices_set.insert(tri1.b);
		vertices_set.insert(tri1.c);
		vertices_set.insert(tri2.a);
		vertices_set.insert(tri2.b);
		vertices_set.insert(tri2.c);

		MicroNode parent;
		parent.id = static_cast<Index>(parent_micronodes.size());
		parent.triangle_count = 0;
		parent.centroid = {0.0f, 0.0f, 0.0f};
		float total_weight = 0.0f;

		for (std::size_t child_idx : vertices_set) {
			parent.children_nodes.push_back(static_cast<Index>(child_idx));
			const MicroNode& child = mesh.micronodes[child_idx];
			float weight = static_cast<float>(child.triangle_count);
			parent.centroid.x += child.centroid.x * weight;
			parent.centroid.y += child.centroid.y * weight;
			parent.centroid.z += child.centroid.z * weight;
			total_weight += weight;
			parent.triangle_count += child.triangle_count;
		}

		if (total_weight > 0.0f) {
			parent.centroid.x /= total_weight;
			parent.centroid.y /= total_weight;
			parent.centroid.z /= total_weight;
		}

		parent.center = parent.centroid;
		parent.radius = 0.0f;
		for (Index child_id : parent.children_nodes) {
			const MicroNode& child = mesh.micronodes[child_id];
			float dx = child.center.x - parent.center.x;
			float dy = child.center.y - parent.center.y;
			float dz = child.center.z - parent.center.z;
			float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
			parent.radius = std::max(parent.radius, dist + child.radius);
		}

		parent_micronodes.push_back(std::move(parent));
	}

	// Step 8: Handle unmatched triangles - each becomes a parent with 3 children
	std::vector<std::size_t> unmatched_triangles;
	for (std::size_t t = 0; t < num_triangle_nodes; ++t) {
		if (!triangle_used[t]) {
			unmatched_triangles.push_back(t);
		}
	}

	std::cout << "Unmatched triangles: " << unmatched_triangles.size() << std::endl;

	// Pair unmatched triangles arbitrarily
	for (std::size_t i = 0; i + 1 < unmatched_triangles.size(); i += 2) {
		std::size_t t1 = unmatched_triangles[i];
		std::size_t t2 = unmatched_triangles[i + 1];

		const Triangle& tri1 = triangles[t1];
		const Triangle& tri2 = triangles[t2];

		std::set<std::size_t> vertices_set;
		vertices_set.insert(tri1.a);
		vertices_set.insert(tri1.b);
		vertices_set.insert(tri1.c);
		vertices_set.insert(tri2.a);
		vertices_set.insert(tri2.b);
		vertices_set.insert(tri2.c);

		MicroNode parent;
		parent.id = static_cast<Index>(parent_micronodes.size());
		parent.triangle_count = 0;
		parent.centroid = {0.0f, 0.0f, 0.0f};
		float total_weight = 0.0f;

		for (std::size_t child_idx : vertices_set) {
			parent.children_nodes.push_back(static_cast<Index>(child_idx));
			const MicroNode& child = mesh.micronodes[child_idx];
			float weight = static_cast<float>(child.triangle_count);
			parent.centroid.x += child.centroid.x * weight;
			parent.centroid.y += child.centroid.y * weight;
			parent.centroid.z += child.centroid.z * weight;
			total_weight += weight;
			parent.triangle_count += child.triangle_count;
		}

		if (total_weight > 0.0f) {
			parent.centroid.x /= total_weight;
			parent.centroid.y /= total_weight;
			parent.centroid.z /= total_weight;
		}

		parent.center = parent.centroid;
		parent.radius = 0.0f;
		for (Index child_id : parent.children_nodes) {
			const MicroNode& child = mesh.micronodes[child_id];
			float dx = child.center.x - parent.center.x;
			float dy = child.center.y - parent.center.y;
			float dz = child.center.z - parent.center.z;
			float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
			parent.radius = std::max(parent.radius, dist + child.radius);
		}

		parent_micronodes.push_back(std::move(parent));
	}

	// Handle last unmatched triangle if odd count
	if (unmatched_triangles.size() % 2 == 1) {
		std::size_t t = unmatched_triangles.back();
		const Triangle& tri = triangles[t];

		MicroNode parent;
		parent.id = static_cast<Index>(parent_micronodes.size());
		parent.children_nodes.push_back(static_cast<Index>(tri.a));
		parent.children_nodes.push_back(static_cast<Index>(tri.b));
		parent.children_nodes.push_back(static_cast<Index>(tri.c));
		parent.triangle_count = 0;
		parent.centroid = {0.0f, 0.0f, 0.0f};
		float total_weight = 0.0f;

		for (Index child_id : parent.children_nodes) {
			const MicroNode& child = mesh.micronodes[child_id];
			float weight = static_cast<float>(child.triangle_count);
			parent.centroid.x += child.centroid.x * weight;
			parent.centroid.y += child.centroid.y * weight;
			parent.centroid.z += child.centroid.z * weight;
			total_weight += weight;
			parent.triangle_count += child.triangle_count;
		}

		if (total_weight > 0.0f) {
			parent.centroid.x /= total_weight;
			parent.centroid.y /= total_weight;
			parent.centroid.z /= total_weight;
		}

		parent.center = parent.centroid;
		parent.radius = 0.0f;
		for (Index child_id : parent.children_nodes) {
			const MicroNode& child = mesh.micronodes[child_id];
			float dx = child.center.x - parent.center.x;
			float dy = child.center.y - parent.center.y;
			float dz = child.center.z - parent.center.z;
			float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
			parent.radius = std::max(parent.radius, dist + child.radius);
		}

		parent_micronodes.push_back(std::move(parent));
	}

	std::cout << "Created " << parent_micronodes.size() << " parent micronodes" << std::endl;

	// Report histogram of children per parent
	std::map<std::size_t, std::size_t> children_hist;
	for (const auto& p : parent_micronodes) {
		children_hist[p.children_nodes.size()]++;
	}
	std::cout << "Children per parent histogram:" << std::endl;
	for (const auto& [n, count] : children_hist) {
		std::cout << "  " << n << " children: " << count << " parents" << std::endl;
	}

	// Store in parent mesh
	parent.micronodes = std::move(parent_micronodes);
}

} // namespace nx
