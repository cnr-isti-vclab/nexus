#ifndef MECO_MESH_H
#define MECO_MESH_H

#include <vcg/complex/complex.h>
#include <vcg/complex/algorithms/update/normal.h>

class CEdge;
class CFace;
class CVertex;

struct CUsedTypes:
 public vcg::UsedTypes<vcg::Use<CVertex>::AsVertexType, vcg::Use<CFace>::AsFaceType> {};

class CVertex: public vcg::Vertex<CUsedTypes, vcg::vertex::Coord3f, vcg::vertex::Normal3f, vcg::vertex::Color4b,
        vcg::vertex::TexCoord2f, vcg::vertex::BitFlags> {
 public:
  bool operator<(const CVertex &v) const {
    return P() < v.P();
  }
};
class CFace  : public vcg::Face<CUsedTypes, vcg::face::VertexRef, vcg::face::FFAdj, vcg::face::BitFlags> {
 public:
  void sort() {
    while(V(0) > V(1) || V(0) > V(2) )
      rotate();
  }
  void rotate() {
    CVertex *t = V(0);
    V(0) = V(1);
    V(1) = V(2);
    V(2) = t;
  }
   bool operator<(const CFace &t) const {
   if(cV(0) == t.cV(0)) {
     if(cV(1) == t.cV(1)) {
//#define SPAN_GAPS
#ifdef SPAN_GAPS
       return cV(2) > t.cV(2);
#else
       return cV(2) < t.cV(2);
#endif
     }
     return cV(1) < t.cV(1);
   }
   return cV(0) < t.cV(0);
 }
 bool operator>(const CFace &t) const {
   if(cV(0) == t.cV(0)) {
     if(cV(1) == t.cV(1)) {
//#define SPAN_GAPS
#ifdef SPAN_GAPS
       return cV(2) < t.cV(2);
#else
       return cV(2) > t.cV(2);
#endif
     }
     return cV(1) > t.cV(1);
   }
   return cV(0) > t.cV(0);
 }
};


class CMesh: public vcg::tri::TriMesh< std::vector<CVertex>, std::vector<CFace> > {};

#endif // MESH_H
