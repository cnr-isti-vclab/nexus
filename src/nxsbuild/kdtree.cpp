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
#include <QDebug>

#include "kdtree.h"
#include "meshstream.h"
#include "mesh.h"
#include "tmesh.h"

using namespace std;

KDTree::KDTree(float adapt): adaptive(adapt) {
	axes[0] = vcg::Point3f(1.0f, 0.0f, 0.0f);
	axes[1] = vcg::Point3f(0.0f, 1.0f, 0.0f);
	axes[2] = vcg::Point3f(0.0f, 0.0f, 1.0f);
}

/* this computes how many bits of the floating point can be used to
 * pick a point within an interval.
 * Moving intervals far away from the origin leaves less bits.
 */

float intervalPrecision(float a, float b) {
	float max = std::max(fabs(a), fabs(b));
	if(max < 1) return 23;
	float diff = fabs(b - a);
	if(diff < 1) return 23;

	float lmax = log2(max);
	float ldiff = log2(diff);

	return ldiff - lmax + 23;


}

float boxFloatPrecision(const vcg::Box3f &box) {
	vcg::Point3f dim = box.Dim();
	vcg::Point3f center = box.Center();
	vcg::Point3f px;
	for(int i = 0; i < 3; i++)
		px[i] = intervalPrecision(box.min[i], box.max[i]);

	float r = std::min(px[0], std::min(px[1], px[2]));
	return r;

	/*	float ux = log2(fabs(center[0])) - 24; //2^ux is the unit at that scale.
	float dx = log2(fabs(dim[0]));   //2^d is the size of the box
	float px = dx - ux; */
}

void KDTree::load(Stream *stream) {

	textures = stream->textures;

	KDCell node;
	node.block = 0;
	//the node.box is relative to the axes
	node.box = computeBox(stream->box);

	float precision = boxFloatPrecision(node.box);
	if(precision < 12) {
			throw QString("Quantiziation is too severe!\n"
			              "The bounding box is far from the origin (with respect to its size),\n"
			              "The model might be quantized.\n"
						  "Try to move the model closer to the origin using -T or -G");
	}

	//Enlarge box to avoid numerical instabilities. (sigh!)
	float offset = node.box.Diag()/100;
	node.box.Offset(vcg::Point3f(offset, offset, offset));

	node.block = addBlock();
	cells.push_back(node);

	loadElements(stream);

	for(quint32 i = 0; i < cells.size(); i++) {
		if(cells[i].isLeaf()) {
			int block = cells[i].block;
			block_boxes[block] = cells[i].box;
		}
	}
}
void KDTree::setAxes(vcg::Point3f &x, vcg::Point3f &y, vcg::Point3f &z) {
	axes[0] = x;
	axes[1] = y;
	axes[2] = z;
}

void KDTree::setAxesDiagonal() {
	ratio = 0.3333;
}

void KDTree::setAxesOrthogonal() {
	ratio = 0.6666;
}

vcg::Box3f KDTree::computeBox(vcg::Box3f &b) {
	vcg::Box3f box;
	//add all corners
	for(uint i = 0; i < 8; i++) {
		vcg::Point3f p;
		for(int k = 0; k < 3; k++) {
			if(i & (1<<k))
				p[k] = b.min[k];
			else
				p[k] = b.max[k];
		}

		volatile float d0 = p*axes[0];
		volatile float d1 = p*axes[1];
		volatile float d2 = p*axes[2];
		box.Add(vcg::Point3f(d0, d1, d2));
	}
	return box;
}


bool KDTree::isInNode(quint32 node, vcg::Point3f &p) {
	vcg::Box3f &box = block_boxes[node];
	return isIn(box, p);
}



bool KDTree::isIn(vcg::Box3f &box, vcg::Point3f &p) {
	return isIn(axes, box, p);
}

bool KDTree::isIn(vcg::Point3f *axes, vcg::Box3f &box, vcg::Point3f &p) {
	for(int i = 0; i < 3; i++) {
		volatile float m = p * axes[i]; //needed to ensure repeatability of the computation
		if(m < box.min[i] || m >= box.max[i]) return false;
	}
	return true;

}//check if point is in the box (using axes) semiopen box.

void KDTree::split(qint32 node_number) {
	KDCell &node = cells[node_number];

	findMiddle(node);

	//create new nodes
	KDCell child0;
	KDCell child1;
	child0.block = node.block;
	child1.block = addBlock();
	node.block = -1;

	child0.box = child1.box = node.box;
	child0.box.max[node.split] = child1.box.min[node.split] = node.middle;

	splitNode(node, child0, child1);

	node.block = -1;
	node.children[0] = cells.size();
	node.children[1] = cells.size() + 1;

	cells.push_back(child0);
	cells.push_back(child1);
}


/* To find the splitting plane we compute the obunding box of the points
   inside the patch (might be different from the node bounding box,
   pick the largest edge and pick the plane that leave half the points on one side */
/* this method would be effective only if triangles arrived at random OR
   triangles arrived at different scales (1 every 10000, then 1 every 1000 then 1 every 100 etc...) */


float KDTree::findMiddle(KDCell &node) {
	if(adaptive == 0.0f) {
		node.split = node.box.MaxDim();
		node.middle = node.box.Center()[node.split];
		return node.middle;
	}

	findRealMiddle(node);

	float min = node.box.min[node.split];
	float max = node.box.max[node.split];
	float interval = max - min;

	float side = node.middle - node.box.min[node.split];
	float ratio = side/interval;
	float min_ratio = (1 - adaptive)/2;
	float max_ratio = (1 - min_ratio);
	assert(min_ratio <= max_ratio);

	if(ratio < min_ratio) node.middle = min + min_ratio*interval;
	if(ratio > max_ratio) node.middle = min + max_ratio*interval;

	return node.middle;
}


void KDTree::lock(Mesh &mesh, int block) {
	vcg::Box3f box = block_boxes[block];
	for(uint32_t i = 0; i < mesh.face.size(); i++) {
		AFace &f = mesh.face[i];
		for(int k = 0; k < 3; k++) {
			if(!isIn(box, f.V(k)->P())) {
				f.ClearW();
				break;
			}
		}
	}
}

void KDTree::lock(TMesh &mesh, int block) {
	vcg::Box3f box = block_boxes[block];
	for(uint32_t i = 0; i < mesh.face.size(); i++) {
		TFace &f = mesh.face[i];
		for(int k = 0; k < 3; k++) {
			if(!isIn(box, f.V(k)->P())) {
				f.ClearW();
				break;
			}
		}
	}
}

//SOUP

void KDTreeSoup::loadElements(Stream *s) {
	StreamSoup *stream = dynamic_cast<StreamSoup *>(s);
	while(true) {
		Soup soup = stream->streamTriangles();
		if(soup.size() == 0) break;
		for(uint i = 0; i < soup.size(); i++) {
			pushTriangle(soup[i]);
		}
	}
	block_boxes.resize(nBlocks());
}

void KDTreeSoup::clear() {
	VirtualTriangleSoup::clear();
	cells.clear();
	block_boxes.clear();
}

void KDTreeSoup::findRealMiddle(KDCell &node) {
	Soup soup = get(node.block);

	vcg::Box3f box;
	for(quint32 i = 0; i < soup.size(); i++) {
		Triangle &triangle = soup[i];
		for(int k = 0; k < 3; k++) {
			vcg::Point3f p(triangle.vertices[k].v);
			/* computation must be repeatable
			  Google for What Every Computer Scientist Should Know About Floating-Point Arithmetic
			  and see
			  Pitfalls in Computations on Extended-Based Systems
			  and cry
			  */
			volatile float d0 = p*axes[0];
			volatile float d1 = p*axes[1];
			volatile float d2 = p*axes[2];
			box.Add(vcg::Point3f(d0, d1, d2));
		}
	}
	//find split axis
	node.split = box.MaxDim();

	vcg::Point3f &axis = axes[node.split];

	std::vector<float> tmp;
	tmp.resize(soup.size());
	//use first vertex of triangle for estimating splitting
	for(quint32 i = 0; i < soup.size(); i++) {
		vcg::Point3f p(soup[i].vertices[0].v);
		tmp[i] = p*axis;
	}
	std::sort(tmp.begin(), tmp.end());
	//node.middle = tmp[soup.size()/2];
	node.middle = tmp[(int)(soup.size()*ratio)];
}

void KDTreeSoup::splitNode(KDCell &node, KDCell &child0, KDCell &child1) {

	Soup source = get(child0.block, true);
	Soup dest = get(child1.block, true);

	quint32 n0 = 0;
	vcg::Point3f axis = axes[node.split];
	for(quint32 i = 0; i < source.size(); i++) {
		Triangle &t = source[i];
		quint32 mask = 0;
		for(int k = 0; k < 3; k ++) {
			vcg::Point3f p(t.vertices[k].v);
			if(isIn(node.box, p))
				mask |= (1<<k);
		}
		int c = 0;
		if(mask != 0)
			c = assign(t, mask, axis, node.middle);
		double w = weight(t);
		if(c == 0) {
			child0.weight += w;
			source[n0++] = t;
		} else {
			child1.weight += w;
			dest.push_back(t);
		}
	}
	source.resize(n0);
//	if(source.size() == 0 || dest.size() == 0)
//		cerr <<  "Degenerate point cloud" << endl;
	drop(child0.block);
	drop(child1.block);
}

void KDTreeSoup::pushTriangle(Triangle &t) {
	/* MOVED TO SIMPLIFICATION AND STREAM, because of pointcloud
	//skip triangle if degenerate
	if(t.vertices[0] == t.vertices[1] || t.vertices[0] == t.vertices[2] || t.vertices[1] == t.vertices[2])
		return; */

	quint32 mask = 0x7;   //all inside
	qint32 node_number = 0;
	do {
		KDCell &node = cells[node_number];
		if(node.isLeaf()) {

			double w = 0;
			if(textures.size() && texelWeight > 0)
				w = weight(t);
			uint32_t node_triangles = occupancy[node.block];
			bool too_large = node_triangles >=triangles_per_block;
			bool too_small = node_triangles < triangles_per_block/16;
			bool too_heavy = node.weight > max_weight;
			if(too_small || (!too_large && !too_heavy)) {
				Soup soup = get(node.block);
				soup.push_back(t);
				node.weight += w;
				break;
			}
			//no space, add 1 block, split node and continue
			//reference to node might not be valid anymore.
			split(node_number);
		} else {
			int c = assign(t, mask, axes[node.split], node.middle);
			node_number = node.children[c];
		}
	} while(1);
}
double KDTreeSoup::weight(Triangle &t) {
	if(textures.size() == 0)
		return 0;
	Vertex &v0 = t.vertices[0];
	Vertex &v1 = t.vertices[1];
	Vertex &v2 = t.vertices[2];

	//TODO deal with negative tex values (or > width/height;
	double w = double(textures[t.tex].width);
	double h = double(textures[t.tex].height);
	double area = fabs(((v1.t[0] - v0.t[0])*(v2.t[1] - v0.t[1]) - (v2.t[0] - v0.t[0])*(v1.t[1] - v0.t[1])))/2.0;
	//compute area in texture space;
	return area*w*h*texelWeight;
}

int KDTreeSoup::assign(Triangle &t, quint32 &mask, vcg::Point3f axis, float middle) {
	int count[] = { 0, 1, 1, 2, 1, 2, 2, 3 };
	quint32 mask0 = 0; //vertices ending up into child0
	quint32 mask1 = 0; //vertices ending up into child1
	assert(mask != 0);
	for(int i = 0; i < 3; i++) {
		if(!(mask & (1<<i))) continue;    //vertex was already outside
		vcg::Point3f p(t.vertices[i].v);
		volatile float r = p*axis; //volatile for repeatability
		if(r >= middle)                   //careful, we have semiopen boxes
			mask1 |= (1<<i);
		else
			mask0 |= (1<<i);
	}
	assert(mask0 + mask1 > 0);
	if(count[mask1] >= count[mask0]) {
		mask = mask1;
		return 1;
	} else {
		mask = mask0;
		return 0;
	}
}


/* POINT CLOUD */

void KDTreeCloud::loadElements(Stream *s) {
	StreamCloud *stream = dynamic_cast<StreamCloud *>(s);
	while(true) {
		Cloud cloud = stream->streamVertices();
		if(cloud.size() == 0) break;
		for(uint i = 0; i < cloud.size(); i++) {
			const vcg::Point3f &p = cloud[i].v;
			pushVertex(cloud[i]);
		}
	}
	block_boxes.resize(nBlocks());
}


void KDTreeCloud::clear() {
	VirtualVertexCloud::clear();
	cells.clear();
	block_boxes.clear();
}

void KDTreeCloud::findRealMiddle(KDCell &node) {
	Cloud cloud = get(node.block);

	vcg::Box3f box;
	for(quint32 i = 0; i < cloud.size(); i++) {
		Vertex &vertex = cloud[i];
		vcg::Point3f p(vertex.v);

		volatile float d0 = p*axes[0];
		volatile float d1 = p*axes[1];
		volatile float d2 = p*axes[2];
		box.Add(vcg::Point3f(d0, d1, d2));
	}
	//find split axis
	node.split = box.MaxDim();

	vcg::Point3f &axis = axes[node.split];

	std::vector<float> tmp;
	tmp.resize(cloud.size());
	//use first vertex of triangle for estimating splitting
	for(quint32 i = 0; i < cloud.size(); i++) {
		vcg::Point3f p(cloud[i].v);
		tmp[i] = p*axis;
	}
	std::sort(tmp.begin(), tmp.end());

	//node.middle = tmp[soup.size()/2];
	node.middle = tmp[(int)(cloud.size()*ratio)];
	if(node.middle == box.min[node.split] || node.middle == box.max[node.split])
		throw "Bad node middle in kdtree.";
}

void KDTreeCloud::splitNode(KDCell &node, KDCell &child0, KDCell &child1) {

	Cloud source = get(child0.block, true);
	Cloud dest = get(child1.block, true);

	quint32 n0 = 0;
	vcg::Point3f axis = axes[node.split];
	for(quint32 i = 0; i < source.size(); i++) {
		Splat &v = source[i];
		vcg::Point3f p(v.v);
		volatile float r = p*axis;
		if(r < node.middle)
			source[n0++] = v;
		else
			dest.push_back(v);
	}
	source.resize(n0);
//	if(source.size() == 0 || dest.size() == 0)
//		throw "Degenerate point cloud";
	drop(child0.block);
	drop(child1.block);
}

void KDTreeCloud::pushVertex(Splat &v) {

	qint32 node_number = 0;
	do {
		KDCell &node = cells[node_number];
		if(node.isLeaf()) {
			if(!isBlockFull(node.block)) {
				Cloud cloud = get(node.block);
				cloud.push_back(v);
				break;
			}
			//no space, add 1 block, split node and continue
			//reference to node might not be valid anymore.
			split(node_number);

		} else {
			vcg::Point3f p(v.v);
			volatile float r = p * axes[node.split];

			int c = 0;
			if(r >= node.middle)
				c = 1;

			node_number = node.children[c];
		}
	} while(1);
}



