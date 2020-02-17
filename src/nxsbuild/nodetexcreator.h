#ifndef NODETEXCREATOR_H
#define NODETEXCREATOR_H

#include <QImage>
#include <vector>
#include "tmesh.h"
#include "texpyramid.h"

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

	NodeTexCreator(nx::TexAtlas &_atlas): atlas(_atlas) {}
	TextureGroup process(TMesh &mesh, int level);
};

#endif // NODETEXCREATOR_H
