#ifndef NX_PLYLOADER_H
#define NX_PLYLOADER_H

#include <wrap/ply/plylib.h>
#include <filesystem>
#include "material.h"
#include "meshloader.h"

namespace nx {
class MeshFiles;

class PlyLoader: public MeshLoader {
public:
	PlyLoader(const std::string& filename);
	~PlyLoader();

	void load(MeshFiles& mesh);

	std::vector<Material> materials;

private:
	vcg::ply::PlyFile pf;
	bool double_coords = false;
	bool has_vertex_tex_coords = false;
	size_t vertices_element;
	size_t faces_element;

	size_t n_vertices;
	size_t n_triangles;

	bool has_normals = false;
	bool has_colors = false;
	bool has_textures = false;
	void init();
};
}
#endif // NX_PLYLOADER_H

