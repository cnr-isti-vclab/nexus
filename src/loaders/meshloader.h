#ifndef NX_LOADERS_MESHLOADER_H
#define NX_LOADERS_MESHLOADER_H

#include "../core/mesh_types.h"
#include "../core/material.h"
#include <vector>
#include <string>
#include <filesystem>
#include <unordered_map>

namespace nx {

class MappedMesh;
/**
 * Base class for mesh loaders (OBJ, PLY, glTF, etc.)
 * Populates a MappedMesh structure with indexed geometry backed by mmapped files.
 */

void load_mesh(const std::filesystem::path& input_path, MappedMesh& mesh);

// Helper to sanitize and resolve texture paths
std::string resolveTexturePath(const std::string& model_path, std::string texture_path);
void sanitizeTextureFilepath (std::string &textureFilepath);

class MeshLoader {
public:
	Vector3d origin = {0, 0, 0};
	Vector3d scale = {1, 1, 1};

	// Materials
	std::vector<Material> materials;
	std::unordered_map<std::string, int32_t> material_map;
};
} // namespace nx

#endif // NX_LOADERS_MESHLOADER_H

