#ifndef NX_GLTFLOADER_H
#define NX_GLTFLOADER_H

#include "meshloader.h"
#include <string>
#include <vector>

namespace nx {

class MappedMesh;

class GltfLoader : public MeshLoader {
public:
	GltfLoader(const std::string& filename);
	void load(MappedMesh& mesh, std::vector<Material>& materials);

private:
	std::string gltf_path;
};

} // namespace nx

#endif // NX_GLTFLOADER_H
