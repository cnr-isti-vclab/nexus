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
#ifndef NX_DAG_H
#define NX_DAG_H

#include "config.h"

#include <vcg/space/point2.h>
#include <vcg/space/sphere3.h>
#include <vcg/space/ray3.h>

#include "cone.h"
#include "signature.h"
#include "material.h"

namespace nx {

#define NEXUS_PADDING 256ll



// Magic is: "Nxs "
struct Header2 {

	Header2(): magic(0x4E787320), version(0), nvert(0), nface(0), n_nodes(0), n_patches(0), n_textures(0) {}
	uint32_t magic;
	uint32_t version;
	uint64_t nvert;            //total number of vertices (at full resolution)
	uint64_t nface;            //total number of faces
	Signature signature;
	uint32_t n_nodes;          //number of nodes in the dag
	uint32_t n_patches;        //number of links (patches) in the dag
	uint32_t n_textures;       //number of textures
	vcg::Sphere3f sphere;      //bounding sphere
};

struct Header3 {
	//this data
	uint32_t magic = 0x4E787320;
	uint32_t version = 0;
	uint32_t json_length = 0;   //needs to be padded to 4 bytes!
	
	//this data is now read from the json.
	uint64_t nvert = 0;            //total number of vertices (at full resolution)
	uint64_t nface = 0;            //total number of faces
	Signature signature;
	uint32_t n_nodes = 0;          //number of nodes in the dag
	uint32_t n_patches = 0;        //number of links (patches) in the dag
	uint32_t n_textures = 0;       //number of textures groups
	vcg::Sphere3f sphere;      //bounding sphere

	std::vector<Material> materials;
	uint32_t index_offset;
	uint32_t index_length;


	//throws on error, returns 0 on success, return the actual number of bytes needed otherwise.
	int read(char *buffer, int length);
	std::vector<char> write();
};


struct Node {
	uint32_t offset = 0; //offset on disk (could be moved), granularity NEXUS_PADDING) //use the function
	uint32_t size = 0;   //in bytes
	uint16_t nvert = 0;
	uint16_t nface = 0;
	float error = 0.0f;
	nx::Cone3s cone;           //cone of normals
	vcg::Sphere3f sphere;      //saturated sphere (extraction)
	float tight_radius = 0.0f;        //tight sphere (frustum culling)
	uint32_t first_patch = 0;      //index of the first patch in the array of patches.
	uint32_t last_patch() { return (this + 1)->first_patch; } //this node is ALWAYS inside an array.
	vcg::Sphere3f tightSphere() { return vcg::Sphere3f(sphere.Center(), tight_radius); }

	uint64_t getBeginOffset() { return uint64_t(offset) * uint64_t(NEXUS_PADDING); }
	uint64_t getEndOffset() { return getBeginOffset() + size; }
	uint64_t getSize() { return size; }
	uint32_t indexByteOffset() { return 0; }
	uint32_t attributesByteOffset() { return (nface*6 + 3) & (~3u); } //assumes short indexes

};

//todo read index node, patch and texrture are different!!!
struct Patch {
	uint32_t node = 0;             //destination node
	uint32_t triangle_offset = 0;  //end of the triangles in the node triangle list. //begin from previous patch.
	uint32_t texture = 0;
	uint32_t material = 0;         //index of the materials
};



//pbr materials will have more than 1 textures. so this is actually a group of textures.
//when loading we don't know the material.
//so [n_maps][size][ jpg ].... [size][jpg]
struct TextureGroup {
	uint32_t offset; //offset on disk (could be moved), granularity NEXUS_PADDING)
	uint32_t size; //in bytes`

	uint64_t getBeginOffset() { return uint64_t(offset) * uint64_t(NEXUS_PADDING); }
	uint64_t getEndOffset() { return getBeginOffset() + size; }
	uint64_t getSize() { return  uint64_t(size); }
};

}//
#endif // NX_DAG_H
