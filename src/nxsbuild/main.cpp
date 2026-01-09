#include <iostream>
#include <string>
#include <filesystem>
#include <ctime>
#include <unistd.h>

#include "../core/mesh.h"
#include "../loaders/meshloader.h"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: nxsbuild <input_mesh>" << std::endl;
        return 1;
    }

    std::string input_file = argv[1];
    nx::MeshFiles mesh;

    // Create a unique temporary directory for the mesh files
    namespace fs = std::filesystem;
    fs::path tmp = fs::temp_directory_path() / ("nexus_build_" + std::to_string(::getpid()) + "_" + std::to_string(static_cast<long>(std::time(nullptr))));
    fs::create_directories(tmp);
    mesh.create(tmp);

    try {
        if (!nx::load_mesh(fs::path(input_file), mesh)) {
            std::cerr << "Failed to load mesh." << std::endl;
            return 1;
        }
    } catch (const std::exception &e) {
        std::cerr << "Error loading mesh: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "Nexus v4 builder initialized." << std::endl;
    return 0;
}
