#ifndef NODETEXCREATOR_H
#define NODETEXCREATOR_H

#include <vector>
#include <map>
#include "tmesh.h"
#include "texpyramid.h"

#include <QImage>

namespace nx {
	class TexAtlas;
}

struct TextureGroup: public std::vector<QImage> {
	float error = 0.f;
	float pixelXEdge = 0.f;
};

class NodeTexCreator {
public:
	bool createPowTwoTex = true;
	nx::TexAtlas &atlas;
	BuildMaterials &materials;
	NodeTexCreator(nx::TexAtlas &_atlas, BuildMaterials &_materials):
		atlas(_atlas), 
		materials(_materials) {}
	
	//Input: takes a mesh where for each triangle we have a material.
	//Output: each triangle has a node a material (the compacted ones!) and a texture group
	//  returns also a set of texture groups.
	//we need to split the node in patches by material AND texture group.
	//we can't have different material and same texture group? actually we could:
	//example same basicColor but different specular.
	std::vector<TextureGroup> process(TMesh &mesh, int level);
};

#endif // NODETEXCREATOR_H
