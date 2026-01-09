#include "mesh.h"

namespace nx {

bool MeshFiles::create(const std::filesystem::path& dir_path) {
    close();
    std::filesystem::create_directories(dir_path);
    dir = dir_path;
    bounds = Aabb{{0,0,0},{0,0,0}};

    // Start empty; callers resize once counts are known.
    if (!mapDataFiles(MappedFile::READ_WRITE)) {
        close();
        return false;
    }
    return true;
}

void MeshFiles::close() {
    positions.close();
    colors.close();
    wedges.close();
    triangles.close();
    material_ids.close();
    adjacency.close();
    dir.clear();
    bounds = Aabb{{0,0,0},{0,0,0}};
}

bool MeshFiles::mapDataFiles(MappedFile::Mode mode) {
    if (!positions.open(pathFor("positions.bin").string(), mode, 0)) return false;
    if (!colors.open(pathFor("colors.bin").string(), mode, 0)) return false;
    if (!wedges.open(pathFor("wedges.bin").string(), mode, 0)) return false;
    if (!triangles.open(pathFor("triangles.bin").string(), mode, 0)) return false;
    if (!material_ids.open(pathFor("material_ids.bin").string(), mode, 0)) return false;
    if (!adjacency.open(pathFor("adjacency.bin").string(), mode, 0)) return false;
    return true;
}

} // namespace nx
