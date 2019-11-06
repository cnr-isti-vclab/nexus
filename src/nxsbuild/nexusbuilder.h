/*
Nexus

Copyright(C) 2012 - Federico Ponchio
ISTI - Italian National Research Council - Visual Computing Lab

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License (http://www.gnu.org/licenses/gpl.txt)
for more details.
*/
#ifndef NX_NEXUSBUILDER_H
#define NX_NEXUSBUILDER_H

#include <vector>

#include <QString>
#include <QFile>
#include <QMutex>

#include <vcg/space/box3.h>

#include "../common/signature.h"
#include "../common/dag.h"
#include "../common/virtualarray.h"
#include "texpyramid.h"



class KDTree;
class KDTreeSoup;
class KDTreeCloud;
class Stream;
class StreamSoup;
class StreamCloud;

class QImage;
class TMesh;

namespace nx {

class Nexus;
}

//Structures used for normalization
struct NodeBox {
	vcg::Point3f axes[3];
	vcg::Box3f box;

	NodeBox() {}
	NodeBox(KDTree *tree, uint32_t block);
	bool isIn(vcg::Point3f &p);
	std::vector<bool> markBorders(nx::Node &node, vcg::Point3f *p, uint16_t *f);
};

class NVertex {
public:
	NVertex(uint32_t b, uint32_t i, vcg::Point3f p, vcg::Point3s *n):
		node(b), index(i), point(p), normal(n) {}
	uint32_t node;
	uint32_t index;
	vcg::Point3f point;
	vcg::Point3s *normal;

	bool operator<(const NVertex &v) const {
		if(point == v.point)
			return node > v.node;  //ascending orde: the first will be the highest node order (the leafest node)
		return point < v.point;
	}
};


class NexusBuilder {
public:
	enum Components { FACES = 1, NORMALS = 2, COLORS = 4, TEXTURES = 8 };
	NexusBuilder(quint32 components);
	NexusBuilder(nx::Signature &sig);

	bool hasNormals() { return header.signature.vertex.hasNormals(); }
	bool hasColors() { return header.signature.vertex.hasColors(); }
	bool hasTextures() { return header.signature.vertex.hasTextures(); }

	bool initAtlas(std::vector<QString> &textures);
	void create(KDTree *input, Stream *output, uint top_node_size);
	void createLevel(KDTree *input, Stream *output, int level);

	void setMaxMemory(quint64 m) {
		max_memory = m;
		chunks.setMaxMemory(m);
		atlas.cache_max = m;
	}
	void setScaling(float s) { scaling = s; }

	void saturate();

	void reverseDag();
	void save(QString filename);

	//void addNode(mesh)
	//    void process(Nexus &input, std::vector<bool> &selection);
	QMutex m_input;
	QMutex m_output;
	QMutex m_builder;
	QMutex m_chunks;

	QFile file;

	VirtualChunks chunks;
	std::vector<NodeBox> boxes; //a box for each node

	nx::Header header;
	std::vector<nx::Node> nodes;
	std::vector<nx::Patch> patches;
	std::vector<nx::Texture> textures;
	std::vector<QString> images;

	quint64 input_pixels, output_pixels;
	nx::TexAtlas atlas;
	QTemporaryFile nodeTex; //texure images for each node stored here.
	quint64 max_memory;

	float scaling;
	bool useNodeTex; //use node textures
	int tex_quality;
	bool createPowTwoTex;
	
	//if too many texel per edge, simplification is inhibited, but don't quit prematurely
	int skipSimplifyLevels = 0;


	QImage extractNodeTex(TMesh &mesh, int level, float &error, float &pixelXedge);
	void invertNodes(); //
	void saturateNode(quint32 n);
	void optimizeNode(quint32 node, uchar *chunk);
	void uniformNormals();
	void appendBorderVertices(uint32_t origin, uint32_t destination, std::vector<NVertex> &vertices);
	quint32 pad(quint32 s);

	void testSaturation();
};

#endif // NX_NEXUSBUILDER_H
