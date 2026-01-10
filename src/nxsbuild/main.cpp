#include <iostream>
#include <string>
#include <filesystem>
#include <random>
#include <sstream>
#include <iomanip>

#include "../core/mesh.h"
#include "../core/spatial_sort.h"
#include "../loaders/meshloader.h"
#include "../loaders/objexporter.h"
#include "../loaders/plyexporter.h"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: nxsbuild <input_mesh>" << std::endl;
        return 1;
    }

    std::string input_file = argv[1];
    nx::MeshFiles mesh;

    // Create cache directory with random suffix
    namespace fs = std::filesystem;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dis(0, 0xFFFFFFFF);
    std::stringstream ss;
    ss << "nxs_cache_" << std::hex << std::setfill('0') << std::setw(8) << dis(gen);
    
    fs::path tmp = fs::current_path() / ss.str();
    fs::create_directories(tmp);
    mesh.create(tmp);

    try {
        nx::load_mesh(fs::path(input_file), mesh);
        
        // Sort mesh spatially for memory coherency
        nx::spatial_sort_mesh(mesh);
        
        // Export PLY with colors to visualize spatial ordering
        fs::path ply_output = fs::current_path() / "test_export_colored.ply";
        nx::export_ply(mesh, ply_output, true);
        std::cout << "Exported colored PLY to:" << ply_output << std::endl;
        
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
