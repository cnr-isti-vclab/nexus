#ifndef NODETEXCREATOR_H
#define NODETEXCREATOR_H

#include <vector>
#include <map>
#include "tmesh.h"
#include "texpyramid.h"
#include "buildmaterial.h"

#include <QImage>

namespace nx {
	class TexAtlas;

}

struct TextureGroupBuild: public std::vector<QImage> {
	int32_t material;
	float error = 0.f;
	float pixelXEdge = 0.f;
};

class NodeTexCreator {
public:
	bool createPowTwoTex = true;
	nx::TexAtlas *atlas = nullptr;
	nx::BuildMaterials *materials = nullptr;

	TextureGroupBuild process(TMesh &mesh, int level);
};

#endif // NODETEXCREATOR_H
