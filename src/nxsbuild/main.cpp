#include <iostream>
#include <string>
#include <filesystem>
#include <vector>
#include <set>
#include <map>
#include <algorithm>

#include <QCoreApplication>

#include "build_parameters.h"
#include "merge_simplify_split.h"
#include "../core/mesh.h"
#include "../core/spatial_sort.h"
#include "../core/adjacency.h"
#include "../core/clustering.h"
#include "../core/clustering_metis.h"
#include "../core/micro_clustering.h"
#include "../core/micro_clustering_metis.h"
#include "../loaders/meshloader.h"
#include "../loaders/objexporter.h"
#include "../loaders/plyexporter.h"

namespace fs = std::filesystem;

// Forward declarations for hierarchical simplification
namespace nx {

// Create new micronodes ensuring no two clusters from same parent are grouped
std::vector<MicroNode> create_next_level_micronodes(
	const std::vector<Index>& cluster_parent_micronode,
	Index num_new_clusters,
	const MeshFiles& mesh);

} // namespace nx

int main(int argc, char *argv[]) {
	QCoreApplication app(argc, argv);
	QCoreApplication::setApplicationName("nxsbuild");
	QCoreApplication::setApplicationVersion("4.0");

	// Parse command line parameters
	nx::BuildParameters params;
	if (!params.parse(argc, argv)) {
		return 1;
	}

	// Print configuration
	params.print();

	// For now, use first input file
	if (params.inputs.isEmpty()) {
		std::cerr << "Error: No input files specified" << std::endl;
		return 1;
	}

	std::string input_file = params.inputs.first().toStdString();

	nx::MeshFiles mesh;

	try {
		nx::load_mesh(fs::path(input_file), mesh);

		// Sort mesh spatially for memory coherency
		nx::spatial_sort_mesh(mesh);

		// Compute face-face adjacency
		nx::compute_adjacency(mesh);

		std::size_t max_triangles = params.faces_per_cluster;
		std::size_t cluster_count;

		if (params.use_metis) {
			cluster_count = nx::build_clusters_metis(mesh, max_triangles);
		} else {
			cluster_count = nx::build_clusters(mesh, max_triangles);
		}

		std::cout << "Built " << cluster_count << " clusters" << std::endl;

		// Create micronodes from clusters using METIS
		mesh.micronodes = nx::create_micronodes_metis(mesh, params.clusters_per_node);
		std::size_t clusters_per_micronode = 4;

		

		// Build dual partition and recluster mesh
		nx::build_dual_partition_and_recluster(mesh, params.clusters_per_node, params.faces_per_cluster);

		fs::path ply_output = fs::current_path() / "test_export_clusters_reclustered.ply";
		nx::export_ply(mesh, ply_output, nx::ColoringMode::ByCluster);
		std::cout << "Exported reclustered PLY to: " << ply_output << std::endl;


		// Level 0 is already created (clusters and micronodes)
		int level = 0;
		std::vector<nx::MicroNode> current_micronodes = mesh.micronodes;

		// Track parent micronode for each cluster (for constraint in step 7)
		std::vector<nx::Index> cluster_parent_micronode(mesh.clusters.size());
		for (nx::Index i = 0; i < current_micronodes.size(); ++i) {
			for (nx::Index cluster_id : current_micronodes[i].cluster_ids) {
				cluster_parent_micronode[cluster_id] = i;
			}
		}

		// Continue until we have very few clusters
		while (current_micronodes.size() > 1) {
			std::cout << "\n--- Level " << level << " -> Level " << (level + 1) << " ---" << std::endl;
			std::cout << "Current micronodes: " << current_micronodes.size() << std::endl;
			std::cout << "Current clusters: " << mesh.clusters.size() << std::endl;

			// TODO: Implement the following steps:

			// Step 1-2: For each micronode, merge its 4 clusters and identify boundaries
			std::cout << "TODO: Step 1-2: Merge clusters within micronodes and lock boundaries" << std::endl;

			// Step 3: Simplify each merged mesh to 50% triangles
			std::cout << "TODO: Step 3: Simplify merged meshes to 50% triangles" << std::endl;

			// Step 4: Split each simplified mesh into 2 new clusters
			std::cout << "TODO: Step 4: Split simplified meshes into 2 clusters each" << std::endl;

			// Step 5: Create new micronodes with parent-mixing constraint
			std::cout << "TODO: Step 5: Create new micronodes avoiding same-parent clusters" << std::endl;

			// For now, break to avoid infinite loop
			break;
		}

		std::cout << "\n=== Hierarchical Simplification Complete ===" << std::endl;

	} catch (const std::exception &e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}

	std::cout << "Nexus v4 builder initialized." << std::endl;
	return 0;
}


