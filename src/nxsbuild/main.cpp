#include <iostream>
#include <string>
#include <filesystem>

#include "../core/mesh.h"
#include "../core/spatial_sort.h"
#include "../core/adjacency.h"
#include "../core/clustering.h"
#include "../loaders/meshloader.h"
#include "../loaders/objexporter.h"
#include "../loaders/plyexporter.h"

namespace fs = std::filesystem;

int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: nxsbuild <input_mesh>" << std::endl;
        return 1;
    }

    std::string input_file = argv[1];
    nx::MeshFiles mesh;

    try {
        nx::load_mesh(fs::path(input_file), mesh);
        
        // Sort mesh spatially for memory coherency
        nx::spatial_sort_mesh(mesh);
        
        // Compute face-face adjacency
        nx::compute_adjacency(mesh);
        
        // Build clusters (Nanite-style, greedy boundary minimization)
        std::size_t max_triangles = 126; // NVidia recommended
        std::size_t cluster_count = nx::build_clusters(mesh, max_triangles);
        std::cout << "Built " << cluster_count << " clusters" << std::endl;
        
        // Export PLY with colors to visualize clustering
        fs::path ply_output = fs::current_path() / "test_export_colored.ply";
        nx::export_ply(mesh, ply_output, nx::ColoringMode::ByCluster);
        std::cout << "Exported cluster-colored PLY to: " << ply_output << std::endl;
        
        // Test export
        fs::path output_file = fs::current_path() / "test_export.obj";
        nx::export_obj(mesh, output_file);
        std::cout << "Exported test file to: " << output_file << std::endl;
        
    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "Nexus v4 builder initialized." << std::endl;
    return 0;
}
