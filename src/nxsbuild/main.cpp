#include <iostream>
#include <string>
#include <filesystem>

#include <QCoreApplication>
#include <QLocale>

#include "build_parameters.h"
#include "../core/json.hpp"
#include "../core/mappedmesh.h"
#include "../core/material.h"
#include "../core/mesh_hierarchy.h"
#include "../loaders/meshloader.h"
#include "export_nxs.h"

namespace fs = std::filesystem;

int main(int argc, char *argv[]) {
	QCoreApplication app(argc, argv);
	QCoreApplication::setApplicationName("nxsbuild");
	QCoreApplication::setApplicationVersion("4.0");
	setlocale(LC_ALL, "C");
	QLocale::setDefault(QLocale::C);

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
			nx::MeshHierarchy hierarchy;

			if (params.resume) {
				// Resume: read levels.json in current directory
				using json = nlohmann::json;
				std::ifstream in("levels.json");
				if (!in.is_open()) {
					throw std::runtime_error("Could not open levels.json in current directory for resume");
				}
				json j; in >> j;
				if (!j.contains("levels") || !j["levels"].is_array()) {
					throw std::runtime_error("Invalid levels.json: missing 'levels' array");
				}

				for (const auto &entry : j["levels"]) {
					if (!entry.contains("dir")) continue;
					std::string dir = entry["dir"].get<std::string>();
					nx::MappedMesh *m = new nx::MappedMesh();
					// Use existing directory
					if (!m->create(std::filesystem::path(dir))) {
						throw std::runtime_error("Failed to open mapped mesh directory: " + dir);
					}
					// load saved state if present
					std::filesystem::path statep = std::filesystem::path(dir) / "state.json";
					if (std::filesystem::exists(statep)) {
						try { m->loadState(statep); } catch (...) { /* ignore load errors */ }
					}
					hierarchy.levels.push_back(m);
				}
			} else {
				// Load the base mesh
				nx::MappedMesh *mesh = new nx::MappedMesh();
				std::vector<nx::Material> materials;
				nx::load_mesh(fs::path(input_file), *mesh, materials);

				// Initialize hierarchy with the preprocessed base mesh
				hierarchy.initialize(mesh, materials);
			}

		// Build the complete hierarchy
		nx::log << "Building mesh hierarchy..." << std::endl;
		hierarchy.build_hierarchy(params);

		nx::ExportNxs exporter;
		exporter.export_nxs(hierarchy, "test.nxs");

	} catch (const std::exception &e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}

	return 0;
}


