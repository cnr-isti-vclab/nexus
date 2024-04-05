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

#include "tmesh.h"
#include <vcg/space/index/kdtree/kdtree.h>
#include <wrap/io_trimesh/import.h>

#include <iostream>

using namespace std;

//using namespace vcg;

void TMesh::loadPly(const QString& filename) {
	int loadmask = 0;
	vcg::tri::io::ImporterPLY<TMesh>::Open(*this, filename.toLocal8Bit().data(), loadmask);
	vcg::tri::UpdateNormal<TMesh>::PerVertexNormalized(*this);
	vcg::tri::UpdateNormal<TMesh>::PerFaceNormalized(*this);
}


void TMesh::load(Soup &soup) {
	vcg::tri::Allocator<TMesh>::AddVertices(*this, soup.size() * 3);
	vcg::tri::Allocator<TMesh>::AddFaces(*this, soup.size());

	for(uint i = 0; i < soup.size(); i++) {
		Triangle &triangle = soup[i];
		TFace &f = face[i];
		for(uint k = 0; k < 3; k++) {
			Vertex &in = triangle.vertices[k];
			TVertex &v = vert[i*3 + k];
			v.P()= vcg::Point3f(in.v[0], in.v[1], in.v[2]);
			v.C() = vcg::Color4b(in.c[0], in.c[1], in.c[2], in.c[3]);
			//v.position = i*3 + k;
			f.V(k) = &v;
			f.WT(k).U() = in.t[0];
			f.WT(k).V() = in.t[1];
			assert(!isnan(in.t[0]));
			assert(!isnan(in.t[1]));
		}
		f.node = triangle.node;
		f.tex = triangle.tex;
	}

	//UNIFICATION PROCESS
	/*
	std::sort(vert.begin(), vert.end(), TVertexCompare());

	std::vector<int> remap(vert.size());
	remap[0] = 0;
	vn = 1;
	for(uint i = 1; i < vert.size(); i++) {
		TVertex &current = vert[i];
		TVertex &previous = vert[vn-1];

		if(current.P() != previous.P()) {
			vert[vn++] = current;
		}
		remap[current.position] = vn-1;
	}
	vert.resize(vn);

	TVertex *begin = &*vert.begin();
	for(uint i = 0; i < face.size(); i++) {
		TFace &f = face[i];
		for(int k = 0; k < 3; k++)
			f.V(k) = begin + remap[f.V(k) - begin];
	}

	savePly("test.ply"); */
	vcg::tri::Clean<TMesh>::RemoveDuplicateVertex(*this);
	vcg::tri::Allocator<TMesh>::CompactVertexVector(*this);
	vcg::tri::Allocator<TMesh>::CompactFaceVector(*this);
	vcg::tri::UpdateNormal<TMesh>::PerVertex(*this);
}

void TMesh::load(Cloud &cloud) {
	vcg::tri::Allocator<TMesh>::AddVertices(*this, cloud.size());

	for(uint i = 0; i < cloud.size(); i++) {
		Splat &splat = cloud[i];
		TVertex &v = vert[i];
		v.P() = vcg::Point3f(splat.v[0], splat.v[1], splat.v[2]);
		v.C() = vcg::Color4b(splat.c[0], splat.c[1], splat.c[2], splat.c[3]);
		v.N() = vcg::Point3f(splat.n[0], splat.n[1], splat.n[2]);
		v.node = splat.node;
	}
}


void TMesh::lock(std::vector<bool> &locked) {
	for(uint i = 0; i < face.size(); i++)
		if(locked[i])
			face[i].ClearW();
}


void TMesh::save(Soup &soup, quint32 node) {
	for(uint i = 0; i < face.size(); i++) {
		Triangle triangle;
		TFace &t = face[i];
		for(uint k = 0; k < 3; k++) {
			Vertex &vertex = triangle.vertices[k];
			TVertex &v = *t.V(k);
			vertex.v[0] = v.P()[0];
			vertex.v[1] = v.P()[1];
			vertex.v[2] = v.P()[2];
			vertex.c[0] = v.C()[0];
			vertex.c[1] = v.C()[1];
			vertex.c[2] = v.C()[2];
			vertex.c[3] = v.C()[3];
		}
		triangle.node = node;
		triangle.tex = t.tex;
		soup.push_back(triangle);
	}
}

void TMesh::getTriangles(Triangle *triangles, quint32 node) {
	int count = 0;
	for(uint i = 0; i < face.size(); i++) {
		TFace &t= face[i];
		if(t.IsD()) continue;

		Triangle &triangle = triangles[count++];
		for(uint k = 0; k < 3; k++) {
			Vertex &vertex = triangle.vertices[k];
			TVertex &v = *t.V(k);
			vertex.v[0] = v.P()[0];
			vertex.v[1] = v.P()[1];
			vertex.v[2] = v.P()[2];
			vertex.c[0] = v.C()[0];
			vertex.c[1] = v.C()[1];
			vertex.c[2] = v.C()[2];
			vertex.c[3] = v.C()[3];
			vertex.t[0] = t.WT(k).U();
			vertex.t[1] = t.WT(k).V();
		}
		triangle.node = node;
		triangle.tex = t.tex;
	}
}

void TMesh::getVertices(Splat *vertices, quint32 node) {
	int count = 0;
	for(uint i = 0; i < vert.size(); i++) {
		TVertex &v = vert[i];
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

float TMesh::simplify(quint32 target_faces, Simplification method) {

	//lock border triangles
	for(uint i = 0; i < face.size(); i++)
		if(!face[i].IsW())
			for(int k = 0; k < 3; k++)
				face[i].V(k)->ClearW();

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

std::vector<TVertex> TMesh::simplifyCloud(quint16 target_faces) {
	vector<TVertex> vertices;
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

float TMesh::averageDistance() {
	vcg::Box3f box;
	for(int i = 0; i < VN(); i++)
		box.Add(vert[i].cP());

	float area = pow(box.Volume(), 2.0/3.0);
	return 8*pow(area/VN(), 0.5);

	if(VN() <= 5) return 0;

	vcg::VertexConstDataWrapper<TMesh> ww(*this);

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

void TMesh::savePly(QString filename) {
	int savemask = vcg::tri::io::Mask::IOM_VERTCOORD | vcg::tri::io::Mask::IOM_VERTNORMAL | vcg::tri::io::Mask::IOM_FACEINDEX;
	vcg::tri::io::ExporterPLY<TMesh>::Save(*this, filename.toStdString().c_str(), savemask, false);
}

void TMesh::savePlyTex(QString filename, QString tex) {
	int savemask = vcg::tri::io::Mask::IOM_VERTCOORD | vcg::tri::io::Mask::IOM_VERTNORMAL | vcg::tri::io::Mask::IOM_FACEINDEX | vcg::tri::io::Mask::IOM_VERTTEXCOORD;
	vcg::tri::io::ExporterPLY<TMesh>::Save(*this, filename.toStdString().c_str(), savemask, false);
}

nx::Node TMesh::getNode()
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
//we have textures stored on wedge texture coordinates, we split vertices where wedges coords are different.
void TMesh::splitSeams(nx::Signature &sig) {

	if(sig.vertex.hasNormals() && sig.face.hasIndex())
		vcg::tri::UpdateNormal<TMesh>::PerVertexNormalized(*this);

	//std::vector<bool> visited(vert.size(), false);
	std::vector<int> next(vert.size(), -1);
	std::vector<TVertex> new_vert(vert.size());
	std::vector<int> new_face;
	std::vector<int> vert_to_tex(vert.size(), -2);
	for(auto &f: face) {
		for(int k = 0; k < 3; k++) {
			int index = f.V(k) - &*vert.begin();

			assert(index >= 0);

			while(true) {
				assert(index < (int)new_vert.size());
//				assert(index < (int)visited.size());
				assert(index >= 0);
				TVertex &nv = new_vert[index];
				if(vert_to_tex[index] == -2) { //first time, just update T.
					nv = *f.V(k);
					nv.T() = f.WT(k);
					vert_to_tex[index] = f.tex;
					break;
				}

				if(vert_to_tex[index] == f.tex && nv.T() == f.WT(k))  //found!
					break;

				if(next[index] == -1) { //ok we have to add a new one.
					int new_index = new_vert.size();
					new_vert.push_back(nv);
					new_vert.back().T() = f.WT(k);
					next[index] = new_index;
					next.push_back(-1);
					vert_to_tex.push_back(f.tex);
					index = new_index;
					break;
				}
				index = next[index];
			}
			assert(new_vert[index].T().U() == f.WT(k).U());
			new_face.push_back(index);
		}
	}
	for(size_t i = 0; i < vert.size(); i++) {
		assert(vert_to_tex[i] != -2);
	}
	vert = new_vert;
	vn = vert.size();
	for(size_t i = 0; i < new_face.size(); i+= 3) {
		TFace &f = face[i/3];
		for(int k = 0; k < 3; k++) {
			assert(f.WT(k).U() == vert[new_face[i+k]].T().U());
			f.V(k) = &vert[new_face[i+k]];
		}
	}
}

quint32 TMesh::serializedSize(nx::Signature &sig) {
	//This should take into account duplicated vertices due to texture seams.
	//let's created the replicated vertices.
	assert(vn == (int)vert.size());
	assert(fn == (int)face.size());
	quint16 nvert = vn;
	quint16 nface = fn;
	quint32 size = nvert*sig.vertex.size() + nface*sig.face.size();
	return size;
}

void TMesh::serialize(uchar *buffer, nx::Signature &sig, std::vector<nx::Patch> &patches) {
	assert(vn == (int)vert.size());
	assert(fn == (int)face.size());

	quint32 current_node = 0;
	//find patches and triangle (splat) offsets
	if(sig.face.hasIndex()) {
		//sort face by node.
		std::sort(face.begin(), face.end());

		//remember triangle_offset is the END of the triangles in the patch
		current_node = face[0].node;
		for(int i = 0; i < fn; i++) {
			TFace &f = face[i];
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
		std::sort(vert.begin(), vert.end(), TVertexNodeCompare());
		current_node = vert[0].node;
		for(int i = 0; i < vn; i++) {
			TVertex &v = vert[i];
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

	vcg::Point3f *c = (vcg::Point3f *)buffer;
	for(int i = 0; i < vn; i++)
		c[i] = vert[i].P();
	buffer += vert.size()*sizeof(vcg::Point3f);

	if(sig.vertex.hasTextures()) {
		vcg::Point2f *tstart = (vcg::Point2f *)buffer;
		for(int i = 0; i < vn; i++) {
			//assert(vert[i].T().P()[0] >= 0.0 && vert[i].T().P()[0] <= 1.0);
			tstart[i] = vert[i].T().P();
		}
		buffer += vert.size() * sizeof(vcg::Point2f);
	}

	if(sig.vertex.hasNormals()) {
		vcg::Point3s *nstart = (vcg::Point3s *)buffer;
		for(int i = 0; i < vn; i++) {
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
		for(int i = 0; i < vn; i++)
			cstart[i] = vert[i].C();
		buffer += vert.size() * sizeof(vcg::Color4b);
	}

	quint16 *faces = (quint16 *)buffer;
	for(int i = 0; i < fn; i++) {
		TFace &f = face[i];
		for(int k = 0; k < 3; k++) {
			TVertex *v = f.V(k);
			faces[i*3 + k] = v - &*vert.begin();
		}
		//cout << endl;
	}

	/*
	buffer += face.size() * 6;
	if(sig.face.hasTextures()) {
		float *texts = (float *)buffer;
		for(uint i = 0; i < face.size(); i++) {
			TFace &f = face[i];
			for(int k = 0; k < 3; k++) {
				texts[i*6 + k*2] = f.WT(k).U();
				texts[i*6 + k*2 + 1] = f.WT(k).V();
			}
		}
		buffer += face.size() * sizeof(float)*6;
	}
	*/
}

vcg::Sphere3f TMesh::boundingSphere() {
	std::vector<vcg::Point3f> vertices(vert.size());
	for(quint32 i = 0; i < vert.size(); i++)
		vertices[i] = vert[i].P();
	vcg::Sphere3f sphere;
	sphere.CreateTight(vertices);
	return sphere;
}

nx::Cone3s TMesh::normalsCone() {
	std::vector<vcg::Point3f> normals;
	normals.reserve(face.size());
	//compute face normals
	for(unsigned int i = 0; i < face.size(); i++) {
		TFace &f = face[i];
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

float TMesh::randomSimplify(quint16 /*target_faces*/) {
	assert(0);
	return -1;
}

float TMesh::quadricSimplify(quint32 target) {
	vcg::tri::UpdateNormal<TMesh>::PerFaceNormalized(*this);
	//degenerate norms will create problems in quadri simplification it seems.
	for(auto &f: face) {
		if(f.N().Norm() < 0.01)
			f.N() = vcg::Point3f(1, 0, 0);
	}
	vcg::tri::UpdateTopology<TMesh>::VertexFace(*this);


	vcg::tri::TriEdgeCollapseQuadricTexParameter qparams;
	//vcg::tri::TriEdgeCollapseQuadricParameter qparams;
	qparams.SetDefaultParams();
	qparams.NormalCheck = true;


	vcg::math::Quadric<double> QZero;
	QZero.SetZero();
	vcg::tri::QuadricTexHelper<TMesh>::QuadricTemp TD3(this->vert,QZero);
	vcg::tri::QuadricTexHelper<TMesh>::TDp3()=&TD3;

	std::vector<std::pair<vcg::TexCoord2<float>, vcg::Quadric5<double> > > qv;

	vcg::tri::QuadricTexHelper<TMesh>::Quadric5Temp TD(this->vert,qv);
	vcg::tri::QuadricTexHelper<TMesh>::TDp()=&TD;



	vcg::LocalOptimization<TMesh> DeciSession(*this, &qparams);

	DeciSession.Init<MyTriEdgeCollapseQTex>();
	DeciSession.SetTargetSimplices(target);
	DeciSession.DoOptimization();
	DeciSession.Finalize<MyTriEdgeCollapseQTex>();

	return edgeLengthError();
}

float TMesh::edgeLengthError() {
	if(!face.size()) return 0;
	float error = 0;
	int recount = 0;
	for(unsigned int i = 0; i < face.size(); i++) {
		TFace &f = face[i];
		if(f.IsD()) continue;
		for(int k = 0; k < 3; k++) {
			float err = (f.cV(k)->cP() - f.cV((k+1)%3)->cP()).SquaredNorm();
			error += err;
			recount++;
		}
	}
	return sqrt(error/recount);
}


