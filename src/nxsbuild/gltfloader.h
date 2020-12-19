#ifndef GLTFLOADER_H
#define GLTFLOADER_H

#include "meshloader.h"
#include "../common/virtualarray.h"

namespace fx {
	namespace gltf {
		class Document;
		class Material;
	}
}

class GltfLoader: public MeshLoader {
public:
	GltfLoader(QString file);
	~GltfLoader();

	void setMaxMemory(quint64 max_memory);
	quint32 getTriangles(quint32 size, Triangle *buffer);
	quint32 getVertices(quint32 size, Splat *vertex);
	fx::gltf::Document *doc = nullptr;

private:
	void cacheVertices();
	nx::BuildMaterial convertMaterial(fx::gltf::Material &m);
	int8_t addTexture(nx::BuildMaterial &m, int8_t &i, std::map<int32_t, int8_t> &remap);


	VirtualArray<Vertex> vertices;

	int32_t current_node = 0;
	int32_t current_mesh = 0;
	int32_t current_primitive = 0; //inside mesh
	int32_t current_triangle = 0; //inside primitive
};

#endif // GLTFLOADER_H
