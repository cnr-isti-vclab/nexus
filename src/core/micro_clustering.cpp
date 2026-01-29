#include "micro_clustering.h"
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

//#define USE_HUNGARIAN 1
#ifdef USE_HUNGARIAN
#include "hungarian.h"
#else
#include <lemon/list_graph.h>
#include <lemon/matching.h>
#endif


using namespace std;
namespace nx {

namespace {

// Maximum weight matching using LEMON library or Hungarian algorithm
// Returns pairs of matched node indices
std::vector<std::pair<std::size_t, std::size_t>> max_weight_matching(
	std::size_t num_nodes,
	const std::vector<idx_t>& xadj,
	const std::vector<idx_t>& adjncy,
	const std::vector<idx_t>& adjwgt) {

#ifdef USE_HUNGARIAN
	// Hungarian algorithm implementation (bipartite matching)
	// For general graphs, we create a bipartite graph by duplicating nodes
	// Left side: nodes 0..num_nodes-1, Right side: nodes 0..num_nodes-1

	std::cout << "Using Hungarian algorithm for matching..." << std::endl;

	// Find maximum edge weight for negation (to convert max-weight to min-weight)
	idx_t max_weight = *std::max_element(adjwgt.begin(), adjwgt.end());

	// Build edge list for Hungarian algorithm
	// Convert to minimum weight problem by negating weights
	std::vector<WeightedBipartiteEdge> edges;
	edges.reserve(adjwgt.size());

	for (std::size_t i = 0; i < num_nodes; ++i) {
		idx_t start = xadj[i];
		idx_t end = xadj[i + 1];
		for (idx_t e = start; e < end; ++e) {
			std::size_t j = static_cast<std::size_t>(adjncy[e]);
			if (i < j) { // Add each edge once
				// Convert max-weight to min-weight by negating (or subtracting from max)
				int cost = static_cast<int>(max_weight - adjwgt[e]);
				edges.emplace_back(static_cast<int>(i), static_cast<int>(j), cost);
			}
		}
	}

	// Run Hungarian algorithm
	std::vector<int> matching = hungarianMinimumWeightPerfectMatching(static_cast<int>(num_nodes), edges);

	// Extract matched pairs
	std::vector<std::pair<std::size_t, std::size_t>> matched_pairs;
	if (!matching.empty()) {
		std::vector<bool> processed(num_nodes, false);
		for (std::size_t i = 0; i < num_nodes; ++i) {
			if (!processed[i] && matching[i] >= 0) {
				std::size_t j = static_cast<std::size_t>(matching[i]);
				if (i < j) {
					matched_pairs.emplace_back(i, j);
					processed[i] = true;
					processed[j] = true;
				}
			}
		}
	}

	return matched_pairs;

#else
	// LEMON library implementation (general graph matching)
	using namespace lemon;

	std::cout << "Using LEMON library for matching..." << std::endl;

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
#endif
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

		//group histo:
		std::unordered_map<std::size_t, std::size_t> histo;
		for(auto &g: final_groups) {
			histo[g.size()]++;
		}
		// Assign partition IDs
		for (std::size_t p = 0; p < final_groups.size(); ++p) {
			for (std::size_t v : final_groups[p]) {
				part[v] = static_cast<idx_t>(p);
			}
		}

		std::cout << " Final groups histogram: ";
		for(auto &kv: histo) {
			std::cout << kv.first << "->" << kv.second << " ";
		}
		std::cout << std::endl;

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
// ...existing code...
// Convert adjacency map to CSR format (weights are doubles here, will be rescaled to int range)
void adjacency_to_csr(const std::vector<std::map<Index, double>>& adjacency,
					  std::vector<idx_t>& xadj,
					  std::vector<idx_t>& adjncy,
					  std::vector<idx_t>& adjwgt) {
	xadj.clear();
	adjncy.clear();
	adjwgt.clear();

	std::size_t num_nodes = adjacency.size();

	// Find maximum weight to scale into [0 .. 1'000'000]
	double max_w = 0.0;
	for (std::size_t i = 0; i < num_nodes; ++i) {
		for (const auto& kv : adjacency[i]) {
			double w = kv.second;
			if (w > max_w) max_w = w;
		}
	}

	const double TARGET_MAX = 1e6;
	double scale = (max_w > 0.0) ? (TARGET_MAX / max_w) : 1.0;

	xadj.reserve(num_nodes + 1);
	xadj.push_back(0);

	for (std::size_t i = 0; i < num_nodes; ++i) {
		for (const auto& [neighbor, weight] : adjacency[i]) {
			adjncy.push_back(static_cast<idx_t>(neighbor));
			double w = weight;
			if (w < 0.0) w = 0.0; // clamp negatives to zero (discourage pairing)
			idx_t iw = static_cast<idx_t>(std::lround(w * scale));
			adjwgt.push_back(iw);
		}
		xadj.push_back(static_cast<idx_t>(adjncy.size()));
	}
}
// ...existing code...

// Build cluster adjacency graph with edge weights based on shared-edge length (shorter -> larger weight)
void build_cluster_graph_weighted(const MeshFiles& mesh,
								  std::vector<idx_t>& xadj,
								  std::vector<idx_t>& adjncy,
								  std::vector<idx_t>& adjwgt,
								  std::vector<idx_t>& vwgt) {
	std::size_t num_clusters = mesh.clusters.size();

	// Use double weights and accumulate per directed pair
	std::vector<std::map<Index, double>> adjacency(num_clusters);

	// Prepare vertex weights: number of triangles per cluster
	vwgt.clear();
	vwgt.reserve(num_clusters);
	for (std::size_t ci = 0; ci < num_clusters; ++ci) {
		idx_t w = static_cast<idx_t>(mesh.clusters[ci].triangle_count);
		if (w < 1) w = 1; // ensure positive weight
		vwgt.push_back(w);
	}

	const double EPS = 1e-6;
	for (std::size_t tri_idx = 0; tri_idx < mesh.triangle_to_cluster.size(); ++tri_idx) {
		Index cluster_id = mesh.triangle_to_cluster[tri_idx];
		const FaceAdjacency& adj = mesh.adjacency[tri_idx];

		const Triangle &tri = mesh.triangles[tri_idx];

		for (int corner = 0; corner < 3; ++corner) {
			Index neighbor_tri = adj.opp[corner];
			if (neighbor_tri != std::numeric_limits<Index>::max()) {
				Index neighbor_cluster = mesh.triangle_to_cluster[neighbor_tri];
				if (neighbor_cluster != cluster_id) {
					// Compute distance between cluster centers (shorter -> larger contribution)
					const Cluster& ca = mesh.clusters[cluster_id];
					const Cluster& cb = mesh.clusters[neighbor_cluster];
					double dx = static_cast<double>(ca.center.x) - static_cast<double>(cb.center.x);
					double dy = static_cast<double>(ca.center.y) - static_cast<double>(cb.center.y);
					double dz = static_cast<double>(ca.center.z) - static_cast<double>(cb.center.z);
					double dist = std::sqrt(dx*dx + dy*dy + dz*dz);
					double contrib = 1.0 / (dist + EPS);
					//double contrib = 1;
					adjacency[cluster_id][neighbor_cluster] += contrib;
				}
			}
		}
	}
	for(size_t i = 0; i < mesh.clusters.size(); i++) {

	}

	// set the weight to minimal to ensure the cluster won't be picked together.
	for(const MicroNode &m: mesh.micronodes) {
		auto &clusters = m.cluster_ids;
		for(Index cluster: clusters) {
			for(auto& [id, weight]: adjacency[cluster]) {
				if(std::find(m.cluster_ids.begin(), m.cluster_ids.end(), id) != m.cluster_ids.end())
					weight /= 1;
			}
		}
	}

	// Convert to CSR format (rescaled to integer range)
	adjacency_to_csr(adjacency, xadj, adjncy, adjwgt);
}

// Build MicroNode objects from partition and mesh
std::vector<MicroNode> build_micronodes_from_partition(const MeshFiles& mesh,
													   const std::vector<idx_t>& part) {

	//find the ACTUAL number of parts:
	std::size_t num_parts = static_cast<std::size_t>(*std::max_element(part.begin(), part.end())) + 1;
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
	const std::vector<idx_t>& vwgt,
	std::size_t num_partitions) {

	idx_t nvtxs = static_cast<idx_t>(num_nodes);
	idx_t ncon = 1;
	idx_t nparts = static_cast<idx_t>(num_partitions);

	std::vector<idx_t> part(num_nodes);

	idx_t options[METIS_NOPTIONS];
	METIS_SetDefaultOptions(options);
	options[METIS_OPTION_OBJTYPE] = METIS_OBJTYPE_CUT;
	options[METIS_OPTION_NUMBERING] = 0;

	int objval;
	int ret = METIS_PartGraphKway(
		&nvtxs, &ncon,
		const_cast<idx_t*>(xadj.data()),
		const_cast<idx_t*>(adjncy.data()),
		const_cast<idx_t*>(vwgt.data()), nullptr,
		const_cast<idx_t*>(adjwgt.data()),
		&nparts,
		nullptr, nullptr,
		options,
		&objval,
		part.data()
		);

	if (ret != METIS_OK) {
		std::cerr << "METIS_PartGraphKway failed with code " << ret << std::endl;
		return {};
	}
	return part;
}

} // anonymous namespace


std::vector<MicroNode> create_micronodes_metis(const MeshFiles& mesh,
											   std::size_t clusters_per_micronode,
											   std::size_t triangles_per_cluster) {
	std::cout << "\n=== Building micronodes ===" << std::endl;

	std::size_t num_clusters = mesh.clusters.size();
	if (num_clusters == 0) {
		throw std::runtime_error("Error: No clusters to partition into micronodes");
	}

	// Estimate number of micronodes
	std::size_t num_micronodes = (num_clusters + clusters_per_micronode - 1) / clusters_per_micronode;

	// Build weighted cluster graph
	std::vector<idx_t> xadj;
	std::vector<idx_t> adjncy;
	std::vector<idx_t> adjwgt;
	std::vector<idx_t> vwgt;

	build_cluster_graph_weighted(mesh, xadj, adjncy, adjwgt, vwgt);

	if (adjncy.empty()) {
		throw std::runtime_error("Error: Cluster graph has no edges (disconnected clusters)");
	}

	idx_t objval = 0;
	std::vector<idx_t> part = partition_graph_metis(num_clusters, xadj, adjncy, adjwgt,
													vwgt,
													num_micronodes);

	if (part.empty()) {
		throw std::runtime_error("Exact-size partitioning failed!");
	}

	// Build MicroNode structures from partition
	std::vector<MicroNode> micronodes = build_micronodes_from_partition(mesh, part);

	return micronodes;
}





} // namespace nx
