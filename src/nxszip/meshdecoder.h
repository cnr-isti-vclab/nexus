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
#ifndef NX_MESHDECODER_H
#define NX_MESHDECODER_H

#include <vector>
#include <algorithm>

#include "cstream.h"
#include "tunstall.h"
#include "fpu_precision.h"
#include "zpoint.h"
#include "../common/nexusdata.h"

namespace meco {

class DEdge2 { //decompression edges
public:
	uint16_t v0;
	uint16_t v1;
	uint16_t v2; //needed for parallelogram prediction
	uint32_t prev, next;
	bool deleted;
	DEdge2(uint16_t a = 0, uint16_t b = 0, uint16_t c = 0, uint32_t p = 0, uint32_t n = 0):
		v0(a), v1(b), v2(c), prev(p), next(n), deleted(false) {}
};

class MeshDecoder {
public:
	enum Clers { VERTEX = 0, LEFT = 1, RIGHT = 2, END = 3, BOUNDARY = 4, DELAY = 5 };
	float error; //quadric error on internal vertices, 0 on boundary vertices
	int coord_q; //global power of 2 quantization.
	int norm_q;  //normal bits
	int color_q[4]; //color bits
	int tex_q;   //texture bits

	CStream stream;

	MeshDecoder(nx::Node &_node, nx::NodeData &_data, nx::Patch *_patches, nx::Signature &_sig):
		node(_node), data(_data), patches(_patches), sig(_sig), vertex_count(0) {}

	void decode(int len, uchar *input);

private:

	nx::Node &node;
	nx::NodeData &data;
	nx::Patch *patches;
	nx::Signature sig;

	vcg::Point3i min, max; //minimum position of vertices
	vcg::Point3i tmin, tmax; //minimum position of texcoords

	int coord_bits; //number of bits for coordinates.
	int tex_bits;  //number of bits for textures

	std::vector<bool> boundary;
	std::vector<int> last;

	void decodeFaces();
	void decodeCoordinates();
	void decodeNormals();
	void decodeColors();

	void shuffle(); //shuffle vertices for point clouds

	void decodeFaces(int start, uint16_t *faces);
	//TODO these are in common with MeshCoder, we should make a meshencoder class and move the common parts
	void computeNormals(vcg::Point3s *estimated_normals);
	void markBoundary();

	int decodeDiff(uchar diff, BitStream &stream);
	//we assume integer computations and float computations are equivalent for numbers < 1<<23
	int decodeVertex(const vcg::Point3i &predicted, const vcg::Point2i &texpredicted, BitStream &bitstream, int diff, int tdiff);

	void dequantize();
	int vertex_count; //keep tracks of current decoding vertex
};

}//namespace
#endif // NX_MESHDECODER_H
