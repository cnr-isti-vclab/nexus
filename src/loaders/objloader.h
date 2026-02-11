#ifndef NX_OBJLOADER_H
#define NX_OBJLOADER_H

#include "meshloader.h"
#include "mesh_types.h"

#include <string>
#include <unordered_map>
#include <vector>
#include <filesystem>


namespace nx {

class MeshFiles;

class ObjLoader: public MeshLoader {
public:
	ObjLoader(const std::string& filename, const std::string& mtl_path = "");

	void load(MeshFiles& mesh);

private:
	void read_mtls();
	void read_mtl(const std::string& mtl_path);

	std::string obj_path;
	bool use_custom_mtl = false;
	std::vector<std::string> mtl_files;

	int32_t current_material_id = -1;
};

} // namespace nx

#endif // NX_OBJLOADER_H
