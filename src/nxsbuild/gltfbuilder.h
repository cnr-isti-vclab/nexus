#ifndef GLTFBUILDER_H
#define GLTFBUILDER_H

#include "deps/tiny_gltf.h"
#include "nexusbuilder.h"

namespace Gltf {
struct Texture {

	std::string uri = "";
	int source = -1;
	int minFilter = TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR;
	int magFilter = TINYGLTF_TEXTURE_FILTER_LINEAR;
};

struct Material {
	vcg::Point3f specular {0, 0, 0};
	vcg::Point3f diffuse {0, 0, 0};
	float shininess  = 0;
	float alpha = 1;
	int texture = -1;
	bool doubleSided = true;
	bool unlit = true;
};
}


class GltfBuilder
{
	public:
	GltfBuilder() = delete;
	GltfBuilder(NexusBuilder& nexus): m_nexus(nexus) {};
	void generateTiles();
	private:
	NexusBuilder &m_nexus;
	tinygltf::Model m_model;
	void clearModel() { m_model = tinygltf::Model();};
	void exportNodeAsTile(int node_index);
	void writeB3DM(const QString & filename);

			// debug
	void writeGLB(const QString &filename);
};

#endif // GLTFBUILDER_H
