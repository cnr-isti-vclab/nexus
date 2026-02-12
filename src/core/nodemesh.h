#ifndef NX_NODEMESH_H
#define NX_NODEMESH_H

#include "mesh_types.h"
#include "material.h"
#include <unordered_map>

namespace nx {

struct NodeMesh {
	std::vector<Vector3f> positions;
	std::vector<Rgba8>    colors;
	std::vector<Vector3f> normals;
	std::vector<Vector2f> texcoords;

	std::vector<Wedge>    wedges;
	std::vector<Triangle> triangles;
	std::vector<Index>    boundary_vertices;          // Vertices on micronode boundary

	std::unordered_map<Index, Index> position_map;    // original position -> merged position
	std::vector<Index>               position_remap;  //merged_position -> original position

	std::vector<Index> material_ids;                  // Optional (size 0 if single/no material)
	Index parent_micronode_id;

	std::vector<TileMap> tilemap; //materials
};

}

#endif //NX_NODEMESH_H
