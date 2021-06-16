#ifndef VCGLOADERMESH_H
#define VCGLOADERMESH_H

#include <vcg/complex/complex.h>


class VcgVertex;
class VcgEdge;
class VcgFace;

struct AUsedTypes: public vcg::UsedTypes<vcg::Use<VcgVertex>::AsVertexType,vcg::Use<VcgEdge>::AsEdgeType,vcg::Use<VcgFace>::AsFaceType>{};

class VcgVertex  : public vcg::Vertex<
		AUsedTypes,
		vcg::vertex::Coord3f,
		vcg::vertex::Normal3f,
		vcg::vertex::TexCoord2f,
		vcg::vertex::Color4b,
		vcg::vertex::BitFlags> {
};

class VcgEdge : public vcg::Edge< AUsedTypes> {};

class VcgFace: public vcg::Face<
		AUsedTypes,
		vcg::face::VertexRef,
		vcg::face::WedgeTexCoord2f,
		vcg::face::BitFlags> {
};

class VcgMesh: public vcg::tri::TriMesh<std::vector<VcgVertex>, std::vector<VcgFace> > {
};
#endif // VCGLOADERMESH_H
