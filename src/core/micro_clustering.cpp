#include "micro_clustering.h"
#include "mesh.h"
#include "mesh_types.h"
#include <vector>
#include <algorithm>
#include <limits>
#include <cmath>
#include <set>
#include <numeric>
#include <iostream>
#include <cassert>

namespace nx {

namespace {

// Helper: Count shared edges between cluster and a micronode
int count_shared_edges_with_micronode(Index cluster_id,
									  const std::vector<Index>& micronode_clusters,
									  const std::vector<std::vector<Index>>& cluster_graph) {
	int shared = 0;

	// Count how many of this cluster's neighbors belong to the micronode
	for (Index neighbor_cluster : cluster_graph[cluster_id]) {
		if (std::find(micronode_clusters.begin(), micronode_clusters.end(), neighbor_cluster)
			!= micronode_clusters.end()) {
			shared++;
		}
	}

	return shared;
}

// Helper: Compute distance between cluster centers
float compute_cluster_distance(const Cluster& c1, const Cluster& c2) {
	float dx = c1.center.x - c2.center.x;
	float dy = c1.center.y - c2.center.y;
	float dz = c1.center.z - c2.center.z;
	return std::sqrt(dx*dx + dy*dy + dz*dz);
}

// Build cluster adjacency graph: clusters connected if they share triangle edges
std::vector<std::vector<Index>> build_cluster_graph(const MeshFiles& mesh) {
	std::size_t num_clusters = mesh.clusters.size();
	std::vector<std::set<Index>> neighbors(num_clusters);

	// Find adjacent clusters via triangle adjacency
	for (std::size_t tri_idx = 0; tri_idx < mesh.triangle_to_cluster.size(); ++tri_idx) {
		Index cluster_id = mesh.triangle_to_cluster[tri_idx];
		const FaceAdjacency& adj = mesh.adjacency[tri_idx];

		for (int corner = 0; corner < 3; ++corner) {
			Index neighbor_tri = adj.opp[corner];
			if (neighbor_tri != std::numeric_limits<Index>::max()) {
				Index neighbor_cluster = mesh.triangle_to_cluster[neighbor_tri];
				if (neighbor_cluster != cluster_id) {
					neighbors[cluster_id].insert(neighbor_cluster);
				}
			}
		}
	}

	// Convert sets to vectors
	std::vector<std::vector<Index>> graph(num_clusters);
	for (std::size_t c = 0; c < num_clusters; ++c) {
		graph[c].assign(neighbors[c].begin(), neighbors[c].end());
	}

	return graph;
}

// Greedy micronode building with boundary minimization
std::vector<MicroNode> build_micronodes_greedy(const MeshFiles& mesh,
											   const std::vector<std::vector<Index>>& cluster_graph,
											   std::size_t max_clusters) {
	std::size_t num_clusters = mesh.clusters.size();
	if (num_clusters == 0) return {};

	// Cluster to micronode mapping
	std::vector<Index> cluster_to_micronode(num_clusters, std::numeric_limits<Index>::max());
	std::vector<MicroNode> micronodes;

	// Front of boundary clusters for next micronode seed selection
	std::set<Index> front;

	// Build micronodes greedily with spatial coherence
	std::size_t clusters_processed = 0;

	while (clusters_processed < num_clusters) {
		// Pick seed: preferentially from front (spatial coherence), otherwise first unprocessed
		Index seed = std::numeric_limits<Index>::max();
		if (!front.empty()) {
			seed = *front.begin();
			assert(cluster_to_micronode[seed] == std::numeric_limits<Index>::max());
		} else {
			for (std::size_t i = 0; i < num_clusters; ++i) {
				if (cluster_to_micronode[i] == std::numeric_limits<Index>::max()) {
					seed = static_cast<Index>(i);
					break;
				}
			}
		}

		assert(seed != std::numeric_limits<Index>::max());

		// Build a new micronode starting from seed
		MicroNode micronode;
		micronode.id = micronodes.size();
		micronode.cluster_ids.clear();
		micronode.triangle_count = 0;
		micronode.centroid = mesh.clusters[seed].center;

		std::set<Index> boundary; // Active boundary of unprocessed adjacent clusters

		// Add seed cluster
		micronode.cluster_ids.push_back(seed);
		micronode.triangle_count += mesh.clusters[seed].triangle_count;
		cluster_to_micronode[seed] = micronode.id;
		clusters_processed++;
		front.erase(seed);

		// Initialize boundary with seed neighbors
		for (Index neighbor : cluster_graph[seed]) {
			if (cluster_to_micronode[neighbor] == std::numeric_limits<Index>::max()) {
				boundary.insert(neighbor);
			}
		}

		// Greedily grow the micronode
		while (micronode.cluster_ids.size() < max_clusters && !boundary.empty()) {
			// Find best candidate from boundary
			Index best = std::numeric_limits<Index>::max();
			float best_score = -std::numeric_limits<float>::max();

			for (Index cluster_id : boundary) {
				assert(cluster_to_micronode[cluster_id] == std::numeric_limits<Index>::max());

				// Score: shared edges (higher is better) minus distance penalty
				int shared = count_shared_edges_with_micronode(cluster_id, micronode.cluster_ids, cluster_graph);
				float distance = compute_cluster_distance(mesh.clusters[cluster_id], mesh.clusters[seed]);
				float score = static_cast<float>(shared) - 0.05f * distance;

				if (score > best_score) {
					best_score = score;
					best = cluster_id;
				}
			}

			if (best == std::numeric_limits<Index>::max()) break;

			// Add the cluster
			micronode.cluster_ids.push_back(best);
			micronode.triangle_count += mesh.clusters[best].triangle_count;
			cluster_to_micronode[best] = micronode.id;
			clusters_processed++;
			boundary.erase(best);
			front.erase(best);

			// Update micronode centroid (weighted by triangle count)
			Vector3f new_center = mesh.clusters[best].center;
			float old_weight = static_cast<float>(micronode.triangle_count - mesh.clusters[best].triangle_count);
			float new_weight = static_cast<float>(mesh.clusters[best].triangle_count);
			float total_weight = old_weight + new_weight;

			micronode.centroid.x = (micronode.centroid.x * old_weight + new_center.x * new_weight) / total_weight;
			micronode.centroid.y = (micronode.centroid.y * old_weight + new_center.y * new_weight) / total_weight;
			micronode.centroid.z = (micronode.centroid.z * old_weight + new_center.z * new_weight) / total_weight;

			// Expand boundary
			for (Index neighbor : cluster_graph[best]) {
				if (cluster_to_micronode[neighbor] == std::numeric_limits<Index>::max()) {
					boundary.insert(neighbor);
				}
			}
		}

		// Add remaining clusters in boundary to front for next seed selection
		for (Index cluster_id : boundary) {
			if (cluster_to_micronode[cluster_id] == std::numeric_limits<Index>::max()) {
				front.insert(cluster_id);
			}
		}

		// Compute bounding sphere from child cluster bounding spheres
		// Use the centroid as initial center, then expand to encompass all child spheres
		micronode.center = micronode.centroid;
		micronode.radius = 0.0f;

		for (Index cluster_id : micronode.cluster_ids) {
			const Cluster& cluster = mesh.clusters[cluster_id];
			// Distance from micronode center to cluster center
			float dx = cluster.center.x - micronode.center.x;
			float dy = cluster.center.y - micronode.center.y;
			float dz = cluster.center.z - micronode.center.z;
			float dist = std::sqrt(dx*dx + dy*dy + dz*dz);
			// Furthest point on this cluster's sphere
			float extent = dist + cluster.radius;
			micronode.radius = std::max(micronode.radius, extent);
		}

		micronodes.push_back(micronode);
	}

	return micronodes;
}

} // anonymous namespace

std::vector<MicroNode> create_micronodes(const MeshFiles& mesh,
										 std::size_t clusters_per_micronode) {
	if (clusters_per_micronode < 1) {
		throw std::runtime_error("clusters_per_micronode must be at least 1");
	}

	std::size_t num_clusters = mesh.clusters.size();
	if (num_clusters == 0) return {};

	std::cout << "\n=== Building Micronodes ===" << std::endl;
	std::cout << "Total clusters: " << num_clusters << std::endl;
	std::cout << "Target clusters per micronode: " << clusters_per_micronode << std::endl;

	// Build cluster adjacency graph
	std::vector<std::vector<Index>> cluster_graph = build_cluster_graph(mesh);

	// Debug: Print graph statistics
	std::size_t total_edges = 0;
	std::size_t isolated_clusters = 0;
	for (std::size_t i = 0; i < cluster_graph.size(); ++i) {
		total_edges += cluster_graph[i].size();
		if (cluster_graph[i].empty()) isolated_clusters++;
	}
	std::cout << "Cluster graph: " << (total_edges / 2) << " edges, "
			  << "avg degree: " << (total_edges / static_cast<float>(num_clusters))
			  << ", isolated: " << isolated_clusters << std::endl;

	// Build micronodes using greedy growth
	std::vector<MicroNode> micronodes = build_micronodes_greedy(mesh, cluster_graph, clusters_per_micronode);

	// Print statistics
	std::size_t min_size = std::numeric_limits<std::size_t>::max();
	std::size_t max_size = 0;
	std::size_t total_size = 0;

	for (const MicroNode& mn : micronodes) {
		std::size_t size = mn.cluster_ids.size();
		min_size = std::min(min_size, size);
		max_size = std::max(max_size, size);
		total_size += size;
	}

	if (!micronodes.empty()) {
		float avg_size = static_cast<float>(total_size) / micronodes.size();
		std::cout << "Created " << micronodes.size() << " micronodes" << std::endl;
		std::cout << "  Size range: [" << min_size << ", " << max_size << "], avg: " << avg_size << std::endl;
	}

	return micronodes;
}

} // namespace nx
