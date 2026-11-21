#ifndef NX_VCGMESH_H
#define NX_VCGMESH_H

#include <vcg/space/point3.h>
#include <vcg/space/color4.h>
#include <QString>

// stuff to define the mesh
#include <vcg/complex/complex.h>
#include <wrap/io_trimesh/export.h> //DEBUG

#include <vcg/math/quadric.h>

// update
#include <vcg/complex/algorithms/clean.h>
#include <vcg/complex/algorithms/update/topology.h>
#include <vcg/complex/algorithms/update/normal.h>

// local optimization
#include <vcg/complex/algorithms/local_optimization.h>
#include <vcg/complex/algorithms/local_optimization/tri_edge_collapse_quadric.h>

//Quadrics simplification structures.
class AVertex;
class AEdge;
class AFace;

struct AUsedTypes: public vcg::UsedTypes<vcg::Use<AVertex>::AsVertexType,vcg::Use<AEdge>::AsEdgeType,vcg::Use<AFace>::AsFaceType>{};

class AVertex  : public vcg::Vertex<
		AUsedTypes,
		vcg::vertex::VFAdj,
		vcg::vertex::Coord3f,
		vcg::vertex::Normal3f,
		vcg::vertex::Color4b,
		vcg::vertex::Mark,
		vcg::vertex::BitFlags> {
public:
	quint32 node;
	AVertex() { q.SetZero(); } //TODO remove this, it's only because of a stupid assert is valid on copy
	//int position;
	vcg::math::Quadric<double> &Qd() {return q;}

private:
	vcg::math::Quadric<double> q;

};

class VertexNodeCompare {
public:
	bool operator()(const AVertex &va, const AVertex &vb) const {
		return va.node < vb.node;
	}
};

class VertexCompare {
public:
	bool operator()(const AVertex &va, const AVertex &vb) const {
		const vcg::Point3f &a = va.cP();
		const vcg::Point3f &b = vb.cP();
		if(a[2] < b[2]) return true;
		if(a[2] > b[2]) return false;
		if(a[1] < b[1]) return true;
		if(a[1] > b[1]) return false;
		return a[0] < b[0];
	}
};

class AEdge : public vcg::Edge< AUsedTypes> {};

class AFace: public vcg::Face<
		AUsedTypes,
		vcg::face::VFAdj,
		vcg::face::VertexRef,
		vcg::face::BitFlags> {
};

class VcgMesh: public vcg::tri::TriMesh<std::vector<AVertex>, std::vector<AFace> > {
public:
	enum Simplification { QUADRICS, EDGE, CLUSTER, RANDOM };

	void lockVertices();

	float simplify(quint16 target_faces, Simplification method);
	std::vector<AVertex> simplifyCloud(quint16 target_vertices); //return removed vertices
	float averageDistance();

	float randomSimplify(quint16 target_faces);
	void quadricInit(); //NOT THREAD SAFE!
	float quadricSimplify(quint16 target_faces);

	float edgeLengthError();

protected:
	vcg::LocalOptimization<VcgMesh> *deciSession = nullptr;
	vcg::tri::TriEdgeCollapseQuadricParameter *qparams = nullptr;
};

typedef vcg::tri::BasicVertexPair<AVertex> VertexPair;

class TriEdgeCollapse:
		public vcg::tri::TriEdgeCollapseQuadric< VcgMesh, VertexPair, TriEdgeCollapse, vcg::tri::QInfoStandard<AVertex>  > {
public:
	typedef  vcg::tri::TriEdgeCollapseQuadric< VcgMesh,  VertexPair, TriEdgeCollapse, vcg::tri::QInfoStandard<AVertex>  > TECQ;
	typedef  VcgMesh::VertexType::EdgeType EdgeType;
	inline TriEdgeCollapse(  const VertexPair &p, int i, vcg::BaseParameterClass *pp) :TECQ(p,i,pp){}
};

#endif // NX_VCGMESH_H
