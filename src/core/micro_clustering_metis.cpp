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
using namespace std;
namespace nx {

namespace {

DualPartitionSplit split_triangles_dual_partition_impl(
	const MeshFiles& mesh,
	const std::vector<Index>& triangle_indices,
	const Vector3f& primary_centroid,
	const Vector3f& dual_centroid) {

	DualPartitionSplit result;
	if (triangle_indices.size() < 2) {
		result.primary_triangles = triangle_indices;
		return result;
	}

	std::unordered_map<Index, Index> tri_to_local;
	tri_to_local.reserve(triangle_indices.size());
	for (std::size_t i = 0; i < triangle_indices.size(); ++i) {
		tri_to_local[triangle_indices[i]] = static_cast<Index>(i);
	}

	std::size_t num_nodes = triangle_indices.size() + 2;
	Index primary_dummy = static_cast<Index>(triangle_indices.size());
	Index dual_dummy = static_cast<Index>(triangle_indices.size() + 1);

	std::vector<idx_t> xadj;
	std::vector<idx_t> adjncy;
	std::vector<idx_t> adjwgt;

	xadj.reserve(num_nodes + 1);
	xadj.push_back(0);

	for (std::size_t i = 0; i < triangle_indices.size(); ++i) {
		Index tri_idx = triangle_indices[i];
		const Triangle& tri = mesh.triangles[tri_idx];
		const FaceAdjacency& adj = mesh.adjacency[tri_idx];

		for (int corner = 0; corner < 3; ++corner) {
			Index neighbor_tri = adj.opp[corner];
			if (neighbor_tri != std::numeric_limits<Index>::max()) {
				auto it = tri_to_local.find(neighbor_tri);
				if (it != tri_to_local.end()) {
					adjncy.push_back(static_cast<idx_t>(it->second));
					adjwgt.push_back(10);
				}
			}
		}

		Vector3f tri_center = {0.0f, 0.0f, 0.0f};
		for (int v = 0; v < 3; ++v) {
			const Vector3f& pos = mesh.positions[mesh.wedges[tri.w[v]].p];
			tri_center.x += pos.x;
			tri_center.y += pos.y;
			tri_center.z += pos.z;
		}
		tri_center.x /= 3.0f;
		tri_center.y /= 3.0f;
		tri_center.z /= 3.0f;

		float dx_p = tri_center.x - primary_centroid.x;
		float dy_p = tri_center.y - primary_centroid.y;
		float dz_p = tri_center.z - primary_centroid.z;
		float dist_primary = std::sqrt(dx_p*dx_p + dy_p*dy_p + dz_p*dz_p);

		float dx_d = tri_center.x - dual_centroid.x;
		float dy_d = tri_center.y - dual_centroid.y;
		float dz_d = tri_center.z - dual_centroid.z;
		float dist_dual = std::sqrt(dx_d*dx_d + dy_d*dy_d + dz_d*dz_d);

		int weight_primary = std::max(1, static_cast<int>(100.0f / (dist_primary + 1.0f)));
		int weight_dual = std::max(1, static_cast<int>(100.0f / (dist_dual + 1.0f)));

		adjncy.push_back(static_cast<idx_t>(primary_dummy));
		adjwgt.push_back(weight_primary);

		adjncy.push_back(static_cast<idx_t>(dual_dummy));
		adjwgt.push_back(weight_dual);

		xadj.push_back(static_cast<idx_t>(adjncy.size()));
	}

	xadj.push_back(static_cast<idx_t>(adjncy.size()));
	xadj.push_back(static_cast<idx_t>(adjncy.size()));

	idx_t nvtxs = static_cast<idx_t>(num_nodes);
	idx_t ncon = 1;
	idx_t nparts = 2;
	idx_t objval;
	std::vector<idx_t> part(num_nodes);

	idx_t options[METIS_NOPTIONS];
	METIS_SetDefaultOptions(options);
	options[METIS_OPTION_OBJTYPE] = METIS_OBJTYPE_CUT;
	options[METIS_OPTION_NUMBERING] = 0;

	int ret = METIS_PartGraphKway(
		&nvtxs, &ncon,
		xadj.data(), adjncy.data(),
		nullptr, nullptr, adjwgt.data(),
		&nparts, nullptr, nullptr,
		options, &objval, part.data()
		);

	if (ret != METIS_OK) {
		result.primary_triangles = triangle_indices;
		return result;
	}

	Index primary_part = part[primary_dummy];
	Index dual_part = part[dual_dummy];

	for (std::size_t i = 0; i < triangle_indices.size(); ++i) {
		Index tri_idx = triangle_indices[i];
		if (part[i] == primary_part) {
			result.primary_triangles.push_back(tri_idx);
		} else if (part[i] == dual_part) {
			result.dual_triangles.push_back(tri_idx);
		} else {
			result.primary_triangles.push_back(tri_idx);
		}
	}

	return result;
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
// If primary_micronodes is provided, removes edges internal to those micronodes (for dual partitioning)
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

	idx_t options[METIS_NOPTIONS];
	METIS_SetDefaultOptions(options);
	options[METIS_OPTION_OBJTYPE] = METIS_OBJTYPE_CUT;
	options[METIS_OPTION_NUMBERING] = 0;

	int ret = METIS_PartGraphKway(
		&nvtxs, &ncon,
		const_cast<idx_t*>(xadj.data()),
		const_cast<idx_t*>(adjncy.data()),
		nullptr, nullptr,
		const_cast<idx_t*>(adjwgt.data()),
		&nparts,
		nullptr, nullptr,
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

DualPartitionSplit split_triangles_dual_partition(
	const MeshFiles& mesh,
	const std::vector<Index>& triangle_indices,
	const Vector3f& primary_centroid,
	const Vector3f& dual_centroid) {
	return split_triangles_dual_partition_impl(mesh, triangle_indices, primary_centroid, dual_centroid);
}

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

	// Call METIS partitioning
	std::cout << "Calling METIS_PartGraphKway with " << num_clusters << " clusters, "
			  << num_micronodes << " micronodes..." << std::endl;

	idx_t objval;
	std::vector<idx_t> part = partition_graph_metis(num_clusters, xadj, adjncy, adjwgt, num_micronodes, objval);

	if (part.empty()) {
		return {};
	}

	std::cout << "METIS partitioning complete. Edge-cut: " << objval << std::endl;

	// Build MicroNode structures from partition
	std::vector<MicroNode> micronodes = build_micronodes_from_partition(mesh, part, num_micronodes);

	// Report statistics
	std::cout << "\nMicronode statistics:" << std::endl;
	std::cout << "  Total micronodes: " << micronodes.size() << std::endl;

	if (!micronodes.empty()) {
		std::size_t min_clusters = std::numeric_limits<std::size_t>::max();
		std::size_t max_clusters = 0;
		std::size_t total_clusters = 0;

		for (const MicroNode& mn : micronodes) {
			std::size_t n = mn.cluster_ids.size();
			min_clusters = std::min(min_clusters, n);
			max_clusters = std::max(max_clusters, n);
			total_clusters += n;
		}

		std::cout << "  Clusters per micronode: min=" << min_clusters
				  << ", max=" << max_clusters
				  << ", avg=" << (total_clusters / micronodes.size()) << std::endl;
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

	// Call METIS for primary partition (micronode graph)
	std::cout << "Calling METIS for primary parent partition..." << std::endl;
	idx_t objval_primary;
	std::vector<idx_t> part_primary = partition_graph_metis(
		num_child_micronodes, micronode_xadj, micronode_adjncy, micronode_adjwgt,
		num_parent_micronodes, objval_primary);

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

	// Call METIS for dual partition (micronode graph)
	std::cout << "Calling METIS for dual parent partition..." << std::endl;
	idx_t objval_dual;
	std::vector<idx_t> part_dual = partition_graph_metis(
		num_child_micronodes, micronode_xadj_ext, micronode_adjncy_ext, micronode_adjwgt_ext,
		num_parent_micronodes, objval_dual);

	// Build dual parent micronodes (children_nodes + centroids)
	std::vector<MicroNode> dual_micronodes =
		build_parent_micronodes_from_partition(mesh, part_dual, num_parent_micronodes);

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
/*
std::vector<Cluster> recluster_mesh_multiconstraint(
	MeshFiles& mesh,
	const std::vector<MicroNode>& primary_micronodes,
	const std::vector<MicroNode>& dual_micronodes) {

	std::cout << "\n=== Reclustering Mesh Using Micronode Centroids ===" << std::endl;

	std::size_t num_triangles = mesh.triangles.size();
	std::size_t num_original_clusters = mesh.clusters.size();

	std::cout << "Original clusters: " << num_original_clusters << std::endl;
	std::cout << "Triangles: " << num_triangles << std::endl;

	// Build cluster-to-micronode mappings
	std::vector<Index> cluster_to_primary(num_original_clusters, Index(-1));
	std::vector<Index> cluster_to_dual(num_original_clusters, Index(-1));

	for (std::size_t p = 0; p < primary_micronodes.size(); ++p) {
		for (Index cluster_id : primary_micronodes[p].cluster_ids) {
			cluster_to_primary[cluster_id] = static_cast<Index>(p);
		}
	}

	for (std::size_t d = 0; d < dual_micronodes.size(); ++d) {
		for (Index cluster_id : dual_micronodes[d].cluster_ids) {
			cluster_to_dual[cluster_id] = static_cast<Index>(d);
		}
	}

	// Process each cluster independently, splitting triangles between primary and dual micronodes
	std::vector<std::vector<Index>> new_cluster_triangles(primary_micronodes.size() + dual_micronodes.size());

	for (std::size_t cluster_id = 0; cluster_id < num_original_clusters; ++cluster_id) {
		const Cluster& cluster = mesh.clusters[cluster_id];
		Index primary_mn = cluster_to_primary[cluster_id];
		Index dual_mn = cluster_to_dual[cluster_id];

		assert(primary_mn != Index(-1) && dual_mn != Index(-1));

		const Vector3f& primary_centroid = primary_micronodes[primary_mn].centroid;
		const Vector3f& dual_centroid = dual_micronodes[dual_mn].centroid;

		// Get triangles in this cluster
		std::vector<Index> cluster_tris;
		for (Index i = 0; i < cluster.triangle_count; ++i) {
			cluster_tris.push_back(cluster.triangle_offset + i);
		}

		if (cluster_tris.size() < 2) {
			// Too few triangles to split, assign to primary
			Index primary_partition_idx = primary_mn;
			for (Index tri_idx : cluster_tris) {
				new_cluster_triangles[primary_partition_idx].push_back(tri_idx);
			}
			continue;
		}

		DualPartitionSplit split = split_triangles_dual_partition_impl(
			mesh, cluster_tris, primary_centroid, dual_centroid);

		Index primary_partition_idx = primary_mn;
		Index dual_partition_idx = primary_micronodes.size() + dual_mn;

		for (Index tri_idx : split.primary_triangles) {
			new_cluster_triangles[primary_partition_idx].push_back(tri_idx);
		}
		for (Index tri_idx : split.dual_triangles) {
			new_cluster_triangles[dual_partition_idx].push_back(tri_idx);
		}
	}

	// Build new clusters and reorder triangles
	std::cout << "\nReordering triangles and building new clusters..." << std::endl;

	std::vector<Triangle> new_triangle_order;
	std::vector<FaceAdjacency> new_adjacency_order;
	new_triangle_order.reserve(num_triangles);
	new_adjacency_order.reserve(num_triangles);

	std::vector<Index> old_to_new_tri(num_triangles);
	std::vector<Cluster> new_clusters;

	Index new_tri_idx = 0;
	for (std::size_t i = 0; i < new_cluster_triangles.size(); ++i) {
		if (new_cluster_triangles[i].empty()) continue;

		Cluster cluster;
		cluster.triangle_offset = new_tri_idx;
		cluster.triangle_count = static_cast<Index>(new_cluster_triangles[i].size());

		for (Index old_tri_idx : new_cluster_triangles[i]) {
			old_to_new_tri[old_tri_idx] = new_tri_idx;
			new_triangle_order.push_back(mesh.triangles[old_tri_idx]);
			new_adjacency_order.push_back(mesh.adjacency[old_tri_idx]);
			new_tri_idx++;
		}

		// Compute bounding sphere
		cluster.center = {0.0f, 0.0f, 0.0f};
		cluster.radius = 0.0f;

		for (Index tri_idx : new_cluster_triangles[i]) {
			const Triangle& tri = mesh.triangles[tri_idx];
			for (int v = 0; v < 3; ++v) {
				const Vector3f& pos = mesh.positions[mesh.wedges[tri.w[v]].p];
				cluster.center.x += pos.x;
				cluster.center.y += pos.y;
				cluster.center.z += pos.z;
			}
		}

		float vertex_count = cluster.triangle_count * 3.0f;
		cluster.center.x /= vertex_count;
		cluster.center.y /= vertex_count;
		cluster.center.z /= vertex_count;

		for (Index tri_idx : new_cluster_triangles[i]) {
			const Triangle& tri = mesh.triangles[tri_idx];
			for (int v = 0; v < 3; ++v) {
				const Vector3f& pos = mesh.positions[mesh.wedges[tri.w[v]].p];
				float dx = pos.x - cluster.center.x;
				float dy = pos.y - cluster.center.y;
				float dz = pos.z - cluster.center.z;
				float dist = std::sqrt(dx*dx + dy*dy + dz*dz);
				cluster.radius = std::max(cluster.radius, dist);
			}
		}

		new_clusters.push_back(cluster);
	}

	// Update adjacency references
	for (std::size_t i = 0; i < new_adjacency_order.size(); ++i) {
		for (int corner = 0; corner < 3; ++corner) {
			Index& opp = new_adjacency_order[i].opp[corner];
			if (opp != std::numeric_limits<Index>::max()) {
				opp = old_to_new_tri[opp];
			}
		}
	}

	// Update mesh arrays
	mesh.triangles.resize(new_triangle_order.size());
	mesh.adjacency.resize(new_adjacency_order.size());
	for (std::size_t i = 0; i < new_triangle_order.size(); ++i) {
		mesh.triangles[i] = new_triangle_order[i];
		mesh.adjacency[i] = new_adjacency_order[i];
	}

	mesh.triangle_to_cluster.resize(num_triangles);
	Index cluster_id = 0;
	for (const Cluster& cluster : new_clusters) {
		for (Index i = 0; i < cluster.triangle_count; ++i) {
			mesh.triangle_to_cluster[cluster.triangle_offset + i] = cluster_id;
		}
		cluster_id++;
	}

	std::cout << "Created " << new_clusters.size() << " new clusters" << std::endl;

	// Update mesh.clusters (handle MappedArray properly)
	mesh.clusters.resize(new_clusters.size());
	for (std::size_t i = 0; i < new_clusters.size(); ++i) {
		mesh.clusters[i] = new_clusters[i];
	}

	std::cout << "\n=== Reclustering Complete ===" << std::endl;

	return new_clusters;
} */

/*
void build_dual_partition_and_recluster(
	MeshFiles& mesh,
	std::size_t clusters_per_micronode) {

	std::cout << "\n=== Building Dual Partition and Reclustering ===" << std::endl;

	// Create primary and dual micronodes using METIS
	auto [primary_micronodes, dual_micronodes] = create_primary_and_dual_micronodes(mesh, clusters_per_micronode);

	std::cout << "Primary micronodes: " << primary_micronodes.size() << std::endl;
	std::cout << "Dual micronodes: " << dual_micronodes.size() << std::endl;


	// Measure primary-dual overlap
	float overlap_fraction = measure_micronode_overlap(mesh, primary_micronodes, dual_micronodes);

	// Multi-constraint reclustering
	std::cout << "\n=== Applying Multi-Constraint Reclustering ===" << std::endl;

	// This function modifies mesh in-place (reorders triangles, updates clusters)
	recluster_mesh_multiconstraint(mesh, primary_micronodes, dual_micronodes);
}*/

} // namespace nx
