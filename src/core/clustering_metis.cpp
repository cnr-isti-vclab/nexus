#include "clustering_metis.h"
#include "clustering.h"
#include "mesh.h"
#include "mesh_types.h"
#include <metis.h>
#include <vector>
#include <algorithm>
#include <limits>
#include <cmath>
#include <iostream>
#include <cassert>
#include <map>
#include "../core/log.h"

namespace nx {

void build_clusters_metis(MeshFiles& mesh, std::size_t max_triangles) {
	nx::debug << "\n=== Building clusters with METIS ===" << std::endl;

	std::size_t num_triangles = mesh.triangles.size();
	if (num_triangles == 0) {
		std::cerr << "Error: No triangles to cluster" << std::endl;
		return 0;
	}

	// Estimate number of partitions
	std::size_t num_partitions = (num_triangles + max_triangles - 1) / max_triangles;
	if (num_partitions < 2) num_partitions = 2; // METIS requires at least 2 partitions

	nx::debug << "Triangles: " << num_triangles << ", Max per cluster: " << max_triangles
			  << ", Target partitions: " << num_partitions << std::endl;

	// Compute triangle centroids for distance-based edge weights
	std::vector<Vector3f> triangle_centroids(num_triangles);
	for (std::size_t i = 0; i < num_triangles; ++i) {
		const Triangle& tri = mesh.triangles[i];
		Vector3f centroid = {0, 0, 0};
		for (int j = 0; j < 3; ++j) {
			const Vector3f& p = mesh.positions[mesh.wedges[tri.w[j]].p];
			centroid.x += p.x;
			centroid.y += p.y;
			centroid.z += p.z;
		}
		centroid.x /= 3.0f;
		centroid.y /= 3.0f;
		centroid.z /= 3.0f;
		triangle_centroids[i] = centroid;
	}

	// Build graph in CSR format for METIS with edge weights
	// Each triangle is a node, edges connect adjacent triangles
	// METIS requires undirected graph: each edge must appear in both adjacency lists
	std::vector<idx_t> xadj;
	std::vector<idx_t> adjncy;
	std::vector<idx_t> adjwgt;  // Edge weights based on spatial distance

	xadj.reserve(num_triangles + 1);
	xadj.push_back(0);

	for (std::size_t i = 0; i < num_triangles; ++i) {
		const FaceAdjacency& adj = mesh.adjacency[i];
		const Vector3f& centroid_i = triangle_centroids[i];

		for (int corner = 0; corner < 3; ++corner) {
			Index neighbor = adj.opp[corner];
			// Add ALL valid neighbors (undirected graph - each edge in both directions)
			if (neighbor != std::numeric_limits<Index>::max()) {
				adjncy.push_back(static_cast<idx_t>(neighbor));

				// Compute edge weight: inverse of distance (closer = higher weight = prefer to keep together)
				const Vector3f& centroid_j = triangle_centroids[neighbor];
				float dx = centroid_i.x - centroid_j.x;
				float dy = centroid_i.y - centroid_j.y;
				float dz = centroid_i.z - centroid_j.z;
				float distance = std::sqrt(dx*dx + dy*dy + dz*dz);

				// Weight: higher for closer triangles (METIS minimizes cut weight, so high weight = prefer not to cut)
				// Scale by 1000 and clamp to avoid overflow and ensure integer weights > 0
				idx_t weight = static_cast<idx_t>(std::max(1.0f, 1000.0f / (1.0f + distance)));
				adjwgt.push_back(weight);
			}
		}
		xadj.push_back(static_cast<idx_t>(adjncy.size()));
	}

	nx::debug << "Graph edges: " << adjncy.size() << " (undirected, with spatial distance weights)" << std::endl;

	// Validate graph structure
	if (xadj.size() != num_triangles + 1) {
		std::cerr << "Error: xadj size mismatch: " << xadj.size() << " vs expected " << (num_triangles + 1) << std::endl;
		return 0;
	}

	// Handle pathological case: graph with no edges
	if (adjncy.empty()) {
		std::cerr << "Error: Graph has no edges - mesh has no adjacency (pathological mesh)" << std::endl;
		return 0;
	}

	// Prepare METIS parameters
	idx_t nvtxs = static_cast<idx_t>(num_triangles);
	idx_t ncon = 1;  // Number of balancing constraints
	idx_t nparts = static_cast<idx_t>(num_partitions);
	idx_t objval;    // Edge-cut or communication volume

	std::vector<idx_t> part(num_triangles);  // Output partition for each triangle

	// METIS options
	idx_t options[METIS_NOPTIONS];
	METIS_SetDefaultOptions(options);
	options[METIS_OPTION_OBJTYPE] = METIS_OBJTYPE_CUT;  // Minimize edge cut
	options[METIS_OPTION_NUMBERING] = 0;                // C-style numbering

	// Call METIS partitioning
	nx::debug << "Calling METIS_PartGraphKway with " << nvtxs << " vertices, "
			  << nparts << " partitions..." << std::endl;

	int ret = METIS_PartGraphKway(
		&nvtxs,           // Number of vertices
		&ncon,            // Number of balancing constraints
		xadj.data(),      // Index pointers for adjacency
		adjncy.data(),    // Adjacency list
		nullptr,          // Vertex weights (nullptr = uniform)
		nullptr,          // Vertex sizes (nullptr = uniform)
		adjwgt.data(),    // Edge weights (spatial distance-based)
		&nparts,          // Number of partitions
		nullptr,          // Target partition weights (nullptr = uniform)
		nullptr,          // Imbalance tolerance (nullptr = default)
		options,          // Options array
		&objval,          // Output: objective value (edge-cut)
		part.data()       // Output: partition for each vertex
		);

	if (ret != METIS_OK) {
		std::cerr << "METIS_PartGraphKway failed with code " << ret << std::endl;
		return 0;
	}

	nx::debug << "METIS partitioning complete. Edge-cut: " << objval << std::endl;

	// Store partition assignments
	mesh.triangle_to_cluster.resize(num_triangles);
	for (std::size_t i = 0; i < num_triangles; ++i) {
		mesh.triangle_to_cluster[i] = static_cast<Index>(part[i]);
	}

	// Count triangles per partition
	std::vector<Index> partition_sizes(num_partitions, 0);
	for (std::size_t i = 0; i < num_triangles; ++i) {
		partition_sizes[part[i]]++;
	}

	// Create cluster metadata
	std::vector<Cluster> temp_clusters(num_partitions);
	for (std::size_t i = 0; i < num_partitions; ++i) {
		temp_clusters[i].triangle_offset = 0;  // Will be set during reordering
		temp_clusters[i].triangle_count = partition_sizes[i];
		temp_clusters[i].center = {0, 0, 0};
		temp_clusters[i].radius = 0;
	}

	// Copy to mesh storage
	mesh.clusters.resize(temp_clusters.size());
	for (std::size_t i = 0; i < temp_clusters.size(); ++i) {
		mesh.clusters[i] = temp_clusters[i];
	}

	// Print initial histogram
	print_cluster_histogram(mesh.clusters, "(METIS output)");

	// Reorder triangles by cluster
	reorder_triangles_by_cluster(mesh);

	// Compute bounds
	compute_cluster_bounds(mesh);

	nx::debug << "\nMETIS clustering complete: " << mesh.clusters.size() << " clusters created" << std::endl;

	return mesh.clusters.size();
}


} // namespace nx
