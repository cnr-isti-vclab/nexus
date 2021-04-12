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
#include <assert.h>

#include <QTextStream>
#include <QDebug>

#include "mesh.h"
#include <vcg/space/index/kdtree/kdtree.h>
#include <iostream>

using namespace std;

//using namespace vcg;

void Mesh::load(Soup &soup) {

	vcg::tri::Allocator<Mesh>::AddVertices(*this, soup.size() * 3);
	vcg::tri::Allocator<Mesh>::AddFaces(*this, soup.size());

	for(uint i = 0; i < soup.size(); i++) {
		Triangle &triangle = soup[i];
		AFace &f = face[i];
		for(uint k = 0; k < 3; k++) {
			Vertex &in = triangle.vertices[k];
			AVertex &v = vert[i*3 + k];
			v.P()= vcg::Point3f(in.v[0], in.v[1], in.v[2]);
			v.C() = vcg::Color4b(in.c[0], in.c[1], in.c[2], in.c[3]);
			//v.position = i*3 + k;
			f.V(k) = &v;
		}
		f.node = triangle.node;
	}

	//UNIFICATION PROCESS
	/*
	std::sort(vert.begin(), vert.end(), VertexCompare());

	std::vector<int> remap(vert.size());
	remap[0] = 0;
	vn = 1;
	for(uint i = 1; i < vert.size(); i++) {
		AVertex &current = vert[i];
		AVertex &previous = vert[vn-1];

		if(current.P() != previous.P()) {
			vert[vn++] = current;
		}
		remap[current.position] = vn-1;
	}
	vert.resize(vn);

	AVertex *begin = &*vert.begin();
	for(uint i = 0; i < face.size(); i++) {
		AFace &f = face[i];
		for(int k = 0; k < 3; k++)
			f.V(k) = begin + remap[f.V(k) - begin];
	}

	savePly("test.ply"); */
	vcg::tri::Clean<Mesh>::RemoveDuplicateVertex(*this);
	vcg::tri::Allocator<Mesh>::CompactVertexVector(*this);
	vcg::tri::Allocator<Mesh>::CompactFaceVector(*this);
	vcg::tri::UpdateNormal<Mesh>::PerVertex(*this);
}

void Mesh::load(Cloud &cloud) {
	vcg::tri::Allocator<Mesh>::AddVertices(*this, cloud.size());

	for(uint i = 0; i < cloud.size(); i++) {
		Splat &splat = cloud[i];
		AVertex &v = vert[i];
		v.P() = vcg::Point3f(splat.v[0], splat.v[1], splat.v[2]);
		v.C() = vcg::Color4b(splat.c[0], splat.c[1], splat.c[2], splat.c[3]);
		v.N() = vcg::Point3f(splat.n[0], splat.n[1], splat.n[2]);
		v.node = splat.node;
	}
}


/*void Mesh::lock(std::vector<bool> &locked) {
	for(uint i = 0; i < face.size(); i++)
		if(locked[i])
			face[i].ClearW();
}*/

void Mesh::lockVertices() {
	//lock border triangles
	for(uint i = 0; i < face.size(); i++)
		if(!face[i].IsW())
			for(int k = 0; k < 3; k++)
				face[i].V(k)->ClearW();
}


void Mesh::save(Soup &soup, quint32 node) {
	for(uint i = 0; i < face.size(); i++) {
		Triangle triangle;
		AFace &t = face[i];
		for(uint k = 0; k < 3; k++) {
			Vertex &vertex = triangle.vertices[k];
			AVertex &v = *t.V(k);
			vertex.v[0] = v.P()[0];
			vertex.v[1] = v.P()[1];
			vertex.v[2] = v.P()[2];
			vertex.c[0] = v.C()[0];
			vertex.c[1] = v.C()[1];
			vertex.c[2] = v.C()[2];
			vertex.c[3] = v.C()[3];
		}
		triangle.node = node;
		soup.push_back(triangle);
	}
}

void Mesh::getTriangles(Triangle *triangles, quint32 node) {
	int count = 0;
	for(uint i = 0; i < face.size(); i++) {
		AFace &t= face[i];
		if(t.IsD()) continue;

		Triangle &triangle = triangles[count++];
		for(uint k = 0; k < 3; k++) {
			Vertex &vertex = triangle.vertices[k];
			AVertex &v = *t.V(k);
			vertex.v[0] = v.P()[0];
			vertex.v[1] = v.P()[1];
			vertex.v[2] = v.P()[2];
			vertex.c[0] = v.C()[0];
			vertex.c[1] = v.C()[1];
			vertex.c[2] = v.C()[2];
			vertex.c[3] = v.C()[3];
		}
		triangle.node = node;
	}
}

void Mesh::getVertices(Splat *vertices, quint32 node) {
	int count = 0;
	for(uint i = 0; i < vert.size(); i++) {
		AVertex &v = vert[i];
		if(v.IsD()) continue;

		Splat &splat = vertices[count++];

		splat.v[0] = v.P()[0];
		splat.v[1] = v.P()[1];
		splat.v[2] = v.P()[2];
		splat.c[0] = v.C()[0];
		splat.c[1] = v.C()[1];
		splat.c[2] = v.C()[2];
		splat.c[3] = v.C()[3];
		splat.n[0] = v.N()[0];
		splat.n[1] = v.N()[1];
		splat.n[2] = v.N()[2];


		splat.node = node;
	}
}

float Mesh::simplify(quint16 target_faces, Simplification method) {

	float error = -1;
	switch(method) {
	case RANDOM: error = randomSimplify(target_faces); break;
	case QUADRICS: error = quadricSimplify(target_faces); break;
	default: throw QString("unknown simplification method");
	}

	//unlock everything
	for(uint i = 0; i < vert.size(); i++)
		vert[i].SetW();
	for(uint i = 0; i < face.size(); i++)
		face[i].SetW();

	return error;
}

std::vector<AVertex> Mesh::simplifyCloud(quint16 target_faces) {
	vector<AVertex> vertices;
	vertices.reserve(vert.size() - target_faces);
	float step = vert.size()/(float)target_faces;
	int count = 0;
	float s = 0;
	for(uint i = 0; i < vert.size() ;i++) {
		uint j = (int)floor(s);
		if(j == i) { //keep point
			vert[count++] = vert[i];
			s += step;
		} else
			vertices.push_back(vert[i]);
	}
	vert.resize(count);
	vn = count;

	//estimate error somewhere else.
	return vertices;
}

float Mesh::averageDistance() {
	vcg::Box3f box;
	for(int i = 0; i < VN(); i++)
		box.Add(vert[i].cP());

	float area = pow(box.Volume(), 2.0/3.0);
	return 8*pow(area/VN(), 0.5);

	if(VN() <= 5) return 0;

	vcg::VertexConstDataWrapper<Mesh> ww(*this);

	vcg::KdTree<float> tree(ww);
	int n_of_neighbors = 5;

	vcg::KdTree<float>::PriorityQueue result;

	float avgDist=0;
	int count = 0;
	for (int j = 0; j < VN(); j++) {
		tree.doQueryK(vert[j].cP(), 5, result);
		int neighbours = result.getNofElements();
		//int neighbours = tree.getNofFoundNeighbors();
		//find max distance
		float m = 0;
		for (int i = 0; i < neighbours; i++) {
			int neightId = result.getIndex(i);//tree.getNeighborId(i);
			float d = Distance(vert[j].cP(), vert[neightId].cP());
			if(d > m) m = d;
		}
		avgDist += m;
		count++;
		//vert[j].Q() = avgDist/=neighbours;
	}
	return avgDist/count;
}

void Mesh::savePly(QString filename) {
	int savemask = vcg::tri::io::Mask::IOM_VERTCOORD | vcg::tri::io::Mask::IOM_VERTNORMAL |
			vcg::tri::io::Mask::IOM_FACEINDEX;
	vcg::tri::io::ExporterPLY<Mesh>::Save(*this, filename.toStdString().c_str(), savemask, false);
}

nx::Node Mesh::getNode()
{
	nx::Node node;
	node.offset = -1;
	node.nface = face.size();
	node.nvert = vert.size();
	node.error = -1;
	node.first_patch = -1;
	node.sphere = boundingSphere();
	node.tight_radius = node.sphere.Radius();
	node.cone = normalsCone();
	return node;
}

quint32 Mesh::serializedSize(const nx::Signature &sig) {
	assert(vn == (int)vert.size());
	assert(fn == (int)face.size());
	quint16 nvert = vn;
	quint16 nface = fn;
	return nvert*sig.vertex.size() + nface*sig.face.size();
}

void Mesh:: serialize(uchar *buffer, nx::Signature &sig, std::vector<nx::Patch> &patches) {


	quint32 current_node = 0;
	//find patches and triangle (splat) offsets
	if(sig.face.hasIndex()) {
		//sort face by node.
		std::sort(face.begin(), face.end());

		//remember triangle_offset is the END of the triangles in the patch
		current_node = face[0].node;
		for(uint i = 0; i < face.size(); i++) {
			AFace &f = face[i];
			if(f.node != current_node) {
				nx::Patch patch;
				patch.node = current_node;
				patch.triangle_offset  = i;
				patch.texture = 0xffffffff;
				patches.push_back(patch);
				current_node  = f.node;
			}
		}
	} else {
		std::sort(vert.begin(), vert.end(), VertexNodeCompare());
		current_node = vert[0].node;
		for(uint i = 0; i < vert.size(); i++) {
			AVertex &v = vert[i];
			if(v.node != current_node) {
				nx::Patch patch;
				patch.node = current_node;
				patch.triangle_offset  = i;
				patch.texture = 0xffffffff;
				patches.push_back(patch);
				current_node  = v.node;
			}
		}
	}


	nx::Patch patch;
	patch.node = current_node;
	if(sig.face.hasIndex()) {
		patch.triangle_offset  = face.size();
	} else {
		patch.triangle_offset  = vert.size();
	}
	patch.texture = 0xffffffff;
	patches.push_back(patch);

	if(sig.vertex.hasNormals() && sig.face.hasIndex())
		vcg::tri::UpdateNormal<Mesh>::PerVertexNormalized(*this);

	vcg::Point3f *c = (vcg::Point3f *)buffer;
	for(uint i = 0; i < vert.size(); i++)
		c[i] = vert[i].P();

	buffer += vert.size()*sizeof(vcg::Point3f);

	//assert(!sig.vertex.hasTextures());

	if(sig.vertex.hasNormals()) {
		vcg::Point3s *nstart = (vcg::Point3s *)buffer;
		for(uint i = 0; i < vert.size(); i++) {
			vcg::Point3f nf = vert[i].N();
			nf.Normalize();
			vcg::Point3s &ns = nstart[i];
			for(int k = 0; k < 3; k++)
				ns[k] = (short)(nf[k]*32766);
		}
		buffer += vert.size() * sizeof(vcg::Point3s);
	}

	if(sig.vertex.hasColors()) {
		vcg::Color4b *cstart = (vcg::Color4b *)buffer;
		for(uint i = 0; i < vert.size(); i++)
			cstart[i] = vert[i].C();
		buffer += vert.size() * sizeof(vcg::Color4b);
	}

	quint16 *faces = (quint16 *)buffer;
	for(uint i = 0; i < face.size(); i++) {
		AFace &f = face[i];
		for(int k = 0; k < 3; k++) {
			AVertex *v = f.V(k);
			faces[i*3 + k] = v - &*vert.begin();
		}
	}
	buffer += face.size() * 6;
}

vcg::Sphere3f Mesh::boundingSphere() {
	std::vector<vcg::Point3f> vertices(vert.size());
	for(quint32 i = 0; i < vert.size(); i++)
		vertices[i] = vert[i].P();
	vcg::Sphere3f sphere;
	sphere.CreateTight(vertices);
	return sphere;
}

nx::Cone3s Mesh::normalsCone() {
	std::vector<vcg::Point3f> normals;
	normals.reserve(face.size());
	//compute face normals
	for(unsigned int i = 0; i < face.size(); i++) {
		AFace &f = face[i];
		vcg::Point3f &v0 = f.V(0)->P();
		vcg::Point3f &v1 = f.V(1)->P();
		vcg::Point3f &v2 = f.V(2)->P();

		vcg::Point3f norm = (v1 - v0) ^ (v2 - v0);
		float len = norm.Norm();
		float l1 = (v1 - v0).Norm();
		float l2 = (v2 - v0).Norm();
		float bigger = l1>l2?l1:l2;
		//skip very thin faces.
		if(qFuzzyCompare(bigger, bigger + len))
			continue;
		norm /= len;
		normals.push_back(norm);
	}
	if(normals.size() == 0) {
		return nx::Cone3s();
	}
	nx::AnchoredCone3f acone;
	acone.AddNormals(normals, 0.95f);

	nx::Cone3s cone;
	cone.Import(acone);
	return cone;
}

float Mesh::randomSimplify(quint16 /*target_faces*/) {
	assert(0);
	return -1;
}

void Mesh::quadricInit() {
	vcg::tri::UpdateTopology<Mesh>::VertexFace(*this);
	qparams = new vcg::tri::TriEdgeCollapseQuadricParameter();
	qparams->NormalCheck = true;
	qparams->QualityQuadric = true;
	deciSession = new vcg::LocalOptimization<Mesh>(*this, qparams);

	deciSession->Init<TriEdgeCollapse>();
}

float Mesh::quadricSimplify(quint16 target) {

	deciSession->SetTargetSimplices(target);
	deciSession->DoOptimization();
	delete deciSession;
	delete qparams;
	return edgeLengthError();
}

float Mesh::edgeLengthError() {
	if(!face.size()) return 0;
	float error = 0;
	int recount = 0;
	for(unsigned int i = 0; i < face.size(); i++) {
		AFace &f = face[i];
		if(f.IsD()) continue;
		for(int k = 0; k < 3; k++) {
			float err = (f.cV(k)->cP() - f.cV((k+1)%3)->cP()).SquaredNorm();
			error += err;
			recount++;
		}
	}
	return sqrt(error/recount);
}


