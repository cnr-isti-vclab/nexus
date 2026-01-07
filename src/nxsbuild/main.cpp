#include <iostream>
#include <string>
#include <vector>

#include "../core/mesh_types.h"
#include "../core/mapped_file.h"

namespace nx {

class MeshLoader {
public:
    bool load(const std::string& filename) {
        std::cout << "Stub: Loading mesh from " << filename << std::endl;
        // Todo: Implement loading logic for Ply, Obj, STL, etc.
        // This will populate a stream of Wedges/Triangles
        return true;
    }
};

} // namespace nx

int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: nxsbuild <input_mesh>" << std::endl;
        return 1;
    }

    std::string input_file = argv[1];
    
    nx::MeshLoader loader;
    if (!loader.load(input_file)) {
        std::cerr << "Failed to load mesh." << std::endl;
        return 1;
    }

    std::cout << "Nexus v4 builder initialized." << std::endl;
    return 0;
}
