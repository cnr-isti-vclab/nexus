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

#include "vcgmesh.h"
#include <vcg/space/index/kdtree/kdtree.h>
#include <iostream>

using namespace std;

//using namespace vcg;

void VcgMesh::lockVertices() {
	exit(0);
	//lock border triangles
	for(uint i = 0; i < face.size(); i++)
		if(!face[i].IsW())
			for(int k = 0; k < 3; k++)
				face[i].V(k)->ClearW();
}

float VcgMesh::simplify(quint16 target_faces, Simplification method) {

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

std::vector<AVertex> VcgMesh::simplifyCloud(quint16 target_faces) {
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

float VcgMesh::averageDistance() {
	vcg::Box3f box;
	for(int i = 0; i < VN(); i++)
		box.Add(vert[i].cP());

	float area = pow(box.Volume(), 2.0/3.0);
	return 8*pow(area/VN(), 0.5);

	if(VN() <= 5) return 0;

	vcg::VertexConstDataWrapper<VcgMesh> ww(*this);

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

float VcgMesh::randomSimplify(quint16 /*target_faces*/) {
	assert(0);
	return -1;
}

void VcgMesh::quadricInit() {
	vcg::tri::UpdateTopology<VcgMesh>::VertexFace(*this);

	qparams = new vcg::tri::TriEdgeCollapseQuadricParameter();
	qparams->NormalCheck = true;
	qparams->QualityQuadric = true;
	deciSession = new vcg::LocalOptimization<VcgMesh>(*this, qparams);

	{
		// static mutex guards this initialization across all threads/instances
		static std::mutex quadric_init_mutex;
		std::lock_guard<std::mutex> lock(quadric_init_mutex);
		deciSession->Init<TriEdgeCollapse>();
	}
}

float VcgMesh::quadricSimplify(quint16 target) {

	deciSession->SetTargetSimplices(target);
	deciSession->DoOptimization();
	delete deciSession;
	delete qparams;
	return edgeLengthError();
}

float VcgMesh::edgeLengthError() {
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


