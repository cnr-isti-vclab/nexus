#include <iostream>
#include <string>
#include <filesystem>
#include <random>
#include <sstream>
#include <iomanip>

#include "../core/mesh.h"
#include "../loaders/meshloader.h"
#include "../loaders/objexporter.h"

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
