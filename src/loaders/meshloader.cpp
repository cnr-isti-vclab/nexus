#include "meshloader.h"
#include "objloader.h"
#include <algorithm>

namespace nx {

bool load_mesh(const std::filesystem::path& input_path, MeshFiles& mesh) {
    auto ext = input_path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == ".obj") {
        ObjLoader loader(input_path.string(), "");
        return loader.load(input_path, mesh);
    }

    // TODO: add PLY, glTF, and other loaders here when available.

    throw std::runtime_error("unsupported mesh format: " + ext);
}

} // namespace nx
