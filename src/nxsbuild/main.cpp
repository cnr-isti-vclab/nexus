#include <iostream>
#include <string>
#include <filesystem>

#include <QCoreApplication>

#include "build_parameters.h"
#include "../core/mesh.h"
#include "../core/mesh_hierarchy.h"
#include "../core/clustering.h"
#include "../core/micro_clustering_metis.h"
#include "../loaders/meshloader.h"

namespace fs = std::filesystem;

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

	try {
		// Load the base mesh
		nx::MeshFiles mesh;
		nx::load_mesh(fs::path(input_file), mesh);

		// Initialize hierarchy with the preprocessed base mesh
		nx::MeshHierarchy hierarchy;
		hierarchy.initialize(std::move(mesh));

		// Build the complete hierarchy
		std::cout << "Building mesh hierarchy..." << std::endl;
		hierarchy.build_hierarchy(params);

		// TODO: Export hierarchy to NXS format


	} catch (const std::exception &e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}

	std::cout << "Nexus v4 builder initialized." << std::endl;
	return 0;
}


