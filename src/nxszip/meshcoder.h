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
#ifndef NX_MESHCODER_H
#define NX_MESHCODER_H

#include <vector>
#include <algorithm>

#include "cstream.h"
#include "tunstall.h"
#include "fpu_precision.h"
#include "zpoint.h"
#include "../common/nexusdata.h"

/* come deve funzionare sto attrezzo:
 *
 *1) quantizza vertici con errore fissato (potenza di 2) e usando le quadriche per quantizzare l'interno(da fare in seguito)
 *   mi servono le facce per farlo a modo. salva i vertici.
 *3 salva le facce.
 *2) salva  normal (stima + diff)
  4) colori: at the moment just difference encoded. Not that nice. could use some smarter move.

 */

//face topology utility structures.

namespace meco {

class McFace {
public:
	uint16_t f[3];
	uint16_t t[3]; //topology: opposite face
	uint16_t i[3]; //index in the opposite face of this face: faces[f.t[k]].t[f.i[k]] = f;
	McFace(uint16_t v0 = 0, uint16_t v1 = 0, uint16_t v2 = 0) {
		f[0] = v0; f[1] = v1; f[2] = v2;
		t[0] = t[1] = t[2] = 0xffff;
	}
	bool operator<(const McFace &face) const {
		if(f[0] < face.f[0]) return true;
		if(f[0] > face.f[0]) return false;
		if(f[1] < face.f[1]) return true;
		if(f[1] > face.f[1]) return false;
		return f[2] < face.f[2];
	}
	bool operator>(const McFace &face) const {
		if(f[0] > face.f[0]) return true;
		if(f[0] < face.f[0]) return false;
		if(f[1] > face.f[1]) return true;
		if(f[1] < face.f[1]) return false;
		return f[2] > face.f[2];
	}
};

class CEdge { //compression edges
public:
	uint16_t face;
	uint16_t side; //opposited to side vertex of face
	uint32_t prev, next;
	bool deleted;
	CEdge(uint16_t f = 0, uint16_t s = 0, uint32_t p = 0, uint32_t n = 0):
		face(f), side(s), prev(p), next(n), deleted(false) {}
};


class McEdge { //topology edges
public:
	uint16_t face, side;
	uint16_t v0, v1;
	bool inverted;
	//McEdge(): inverted(false) {}
	McEdge(uint16_t _face, uint16_t _side, uint16_t _v0, uint16_t _v1): face(_face), side(_side), inverted(false) {

		if(_v0 < _v1) {
			v0 = _v0; v1 = _v1;
			inverted = false;
		} else {
			v1 = _v0; v0 = _v1;
			inverted = true;
		}
	}
	bool operator<(const McEdge &edge) const {
		if(v0 < edge.v0) return true;
		if(v0 > edge.v0) return false;
		return v1 < edge.v1;
	}
	bool match(const McEdge &edge) {
		if(inverted && edge.inverted) return false;
		if(!inverted && !edge.inverted) return false;
		//if(!(inverted ^ edge.inverted)) return false;
		return v0 == edge.v0 && v1 == edge.v1;
	}
};


class MeshEncoder {
public:
	enum Clers { VERTEX = 0, LEFT = 1, RIGHT = 2, END = 3, BOUNDARY = 4, DELAY = 5 };

	float error; //quadric error on internal vertices, 0 on boundary vertices
	int coord_q; //global power of 2 quantization.
	int norm_q;  //normal bits
	int color_q[4]; //color bits
	int tex_q;   //texture bits

	CStream stream;
	//compression stats
	int coord_size;
	int normal_size;
	int color_size;
	int face_size;

public:
	MeshEncoder(nx::Node &_node, nx::NodeData &_data, nx::Patch *_patches, nx::Signature &_sig):
		coord_size(0), normal_size(0), color_size(0), face_size(0),
		node(_node), data(_data), patches(_patches), sig(_sig) {}

	void encode();

private:

	nx::Node &node;
	nx::NodeData &data;
	nx::Patch *patches;
	nx::Signature sig;

	vcg::Point3i min, max; //minimum position of vertices
	vcg::Point2i tmin, tmax; //minimum position texture coordinates

	int coord_bits; //number of bits for coordinates.
	int tex_bits; //bumber of bits for texture coordinates

	std::vector<vcg::Point3i> qpoints;
	std::vector<vcg::Point2i> qtexcoords;

	std::vector<ZPoint> zpoints; //used by point cloud only
	std::vector<int> order; //not used for point cloud for mesh we store for each new vertex the original vertex index.
	std::vector<int> reorder; //for each OLD vertex its new vertex

	std::vector<int> last; //used with order to make diffs in colors (refers to the original indexes too.
	std::vector<bool> boundary;
	std::vector<int> encoded; //encoded vertices, use index instead of diff for those.

	static vcg::Point3i quantize(vcg::Point3f &p, float side); //used in encode faces
	void quantize(); //used for point clouds
	void quantizeCoords();
	void quantizeTexCoords();

	void encodeFaces();
	void encodeCoordinates(); //used only for point clouds
	void encodeNormals();
	void encodeColors();

	void encodeFaces(int start, int end);

	void computeNormals(std::vector<vcg::Point3s> &estimated_normals);
	void markBoundary();

	void encodeVertex(int target, const vcg::Point3i &predicted, const vcg::Point2i &texpredicted, BitStream &bitstream, vector<uchar> &diffs, vector<uchar> &tdiffs);
	void encodeDiff(std::vector<uchar> &diffs, BitStream &stream, int val);
};

} //namespace
#endif // NX_MESHCODER_H
