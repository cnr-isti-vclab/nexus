#include "micro_clustering.h"
#include "mappedmesh.h"
#include "../src/core/log.h"

#include <metis.h>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <limits>
#include <cmath>
#include <iostream>
#include <cassert>
#include <queue>
#include <fstream>
#include <filesystem>

using namespace std;
namespace nx {

namespace {

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
void build_cluster_graph_weighted(const MappedMesh& mesh,
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
std::vector<MicroNode> build_micronodes_from_partition(const MappedMesh& mesh,
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

	

	// Sort micronodes by first cluster id (clusters are expected to be ordered)
	std::sort(micronodes.begin(), micronodes.end(), [](const MicroNode& a, const MicroNode& b) {
		return a.cluster_ids.front() < b.cluster_ids.front();
	});
	for (Index i = 0; i < micronodes.size(); ++i) {
		micronodes[i].id = i;
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


std::vector<MicroNode> create_micronodes_metis(const MappedMesh& mesh,
											   std::size_t clusters_per_micronode,
											   std::size_t triangles_per_cluster) {
	nx::debug << "\n=== Building micronodes ===" << std::endl;

	std::size_t num_clusters = mesh.clusters.size();
	if (num_clusters == 0) {
		throw std::runtime_error("Error: No clusters to partition into micronodes");
	}

	// Estimate number of micronodes
	std::size_t num_micronodes = (num_clusters + clusters_per_micronode - 1) / clusters_per_micronode;
	if (num_micronodes <= 1) {
		std::vector<idx_t> part(num_clusters, 0);
		return build_micronodes_from_partition(mesh, part);
	}

	
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
