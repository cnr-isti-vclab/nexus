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
#include "plyloader.h"

using namespace vcg;
using namespace vcg::ply;

#include <iostream>
using namespace std;

//Ugly ply stuff from vcg.

struct PlyFace {
	quint32 f[3];
	float t[6];
	quint32 texNumber;
	unsigned char n;
};

struct PlyVertex {
	double dv[3];
	float v[3];
	float t[2]; //texture
	float n[3];
	unsigned char c[4]; //colors
};

//TODO add uv?
PropDescriptor plyprop1[13]= {
	{"vertex", "x",     T_FLOAT, T_FLOAT, offsetof(PlyVertex,v[0]),0,0,0,0,0,0},
	{"vertex", "y",     T_FLOAT, T_FLOAT, offsetof(PlyVertex,v[1]),0,0,0,0,0,0},
	{"vertex", "z",     T_FLOAT, T_FLOAT, offsetof(PlyVertex,v[2]),0,0,0,0,0,0},
	{"vertex", "red"  , T_UCHAR, T_UCHAR, offsetof(PlyVertex,c[0]),0,0,0,0,0,0},
	{"vertex", "green", T_UCHAR, T_UCHAR, offsetof(PlyVertex,c[1]),0,0,0,0,0,0},
	{"vertex", "blue" , T_UCHAR, T_UCHAR, offsetof(PlyVertex,c[2]),0,0,0,0,0,0},
	{"vertex", "alpha", T_UCHAR, T_UCHAR, offsetof(PlyVertex,c[3]),0,0,0,0,0,0},
	{"vertex", "nx",    T_FLOAT, T_FLOAT, offsetof(PlyVertex, n[0]),0,0,0,0,0,0},
	{"vertex", "ny",    T_FLOAT, T_FLOAT, offsetof(PlyVertex, n[1]),0,0,0,0,0,0},
	{"vertex", "nz",    T_FLOAT, T_FLOAT, offsetof(PlyVertex, n[2]),0,0,0,0,0,0},
	{"vertex", "diffuse_red",   T_UCHAR, T_UCHAR, offsetof(PlyVertex,c[0]),0,0,0,0,0,0},
	{"vertex", "diffuse_green", T_UCHAR, T_UCHAR, offsetof(PlyVertex,c[1]),0,0,0,0,0,0},
	{"vertex", "diffuse_blue" , T_UCHAR, T_UCHAR, offsetof(PlyVertex,c[2]),0,0,0,0,0,0},
};
PropDescriptor doublecoords[3] = {
	{"vertex", "x",     T_DOUBLE, T_DOUBLE, offsetof(PlyVertex,dv[0]),0,0,0,0,0,0},
	{"vertex", "y",     T_DOUBLE, T_DOUBLE, offsetof(PlyVertex,dv[1]),0,0,0,0,0,0},
	{"vertex", "z",     T_DOUBLE, T_DOUBLE, offsetof(PlyVertex,dv[2]),0,0,0,0,0,0}
};

PropDescriptor vindex[1]=	{
	{"face", "vertex_indices",T_INT,T_UINT,offsetof(PlyFace,f[0]),
	 1,0,T_UCHAR,T_UCHAR, offsetof(PlyFace,n) ,0}
};

PropDescriptor vindex_uint[1]=	{
	{"face", "vertex_indices",T_UINT,T_UINT,offsetof(PlyFace,f[0]),
	 1,0,T_UCHAR,T_UCHAR, offsetof(PlyFace,n) ,0}
};

PropDescriptor plyprop3[1]=	{
	{"face", "vertex_index",T_INT,T_UINT,offsetof(PlyFace,f[0]),
	 1,0,T_UCHAR,T_UCHAR, offsetof(PlyFace,n) ,0}
};

PropDescriptor plyprop3_uint[1]=	{
	{"face", "vertex_index",T_UINT,T_UINT,offsetof(PlyFace,f[0]),
	 1,0,T_UCHAR,T_UCHAR, offsetof(PlyFace,n) ,0}
};

PropDescriptor plyprop4[1]=	{
	{"face", "texcoord",T_FLOAT,T_FLOAT,offsetof(PlyFace,t[0]),
	 1,0,T_UCHAR,T_UCHAR, offsetof(PlyFace,n) ,0}
};

PropDescriptor plyprop5[1]=	{
	{"face", "texnumber",T_INT,T_INT,offsetof(PlyFace, texNumber), 0,0,0,0,0,0}
};


PlyLoader::PlyLoader(QString filename):
	vertices_element(-1),
	faces_element(-1),
	vertices("cache_plyvertex"),
	n_vertices(0),
	n_triangles(0),
	current_triangle(0),
	current_vertex(0) {

	int val = pf.Open(filename.toLatin1().data(), PlyFile::MODE_READ);
	if(val == -1) {
		int error = pf.GetError();
		throw QString("could not open file " + filename + ". Error: %1").arg(error);

	}
	init();
	for(int co = 0; co < int(pf.comments.size()); ++co) {
		std::string TFILE = "TEXTUREFILE";
		std::string &c = pf.comments[co];
		std::string bufstr,bufclean;
		int i,n;

		std::string start = c.substr(0,TFILE.length());
		for_each(start.begin(), start.end(), [](char& in){ in = ::toupper(in); });

		if( TFILE == start )
		{
			bufstr = c.substr(TFILE.length()+1);
			n = static_cast<int>(bufstr.length());
			for(i=0;i<n;i++)
				if(bufstr[i]!='\t' && bufstr[i]>=32 && bufstr[i]<=126 )	bufclean.push_back(bufstr[i]);

			char buf2[255];
			ply::interpret_texture_name( bufclean.c_str(),filename.toLatin1().data(), buf2 );
			texture_filenames.push_back(QString(buf2).trimmed());
		}
	}
	if(has_textures && texture_filenames.size() == 0)
		has_textures = false;
}

PlyLoader::~PlyLoader() {
	pf.Destroy();
}

void PlyLoader::init() {
	bool has_faces = false;
	for(unsigned int i = 0; i < pf.elements.size(); i++) {

		if(!strcmp(pf.ElemName(i),"vertex")) {
			n_vertices = pf.ElemNumber(i);
			vertices_element = i;

		} else if( !strcmp(pf.ElemName(i),"face") ) {
			n_triangles = pf.ElemNumber(i);
			if (n_triangles) {
				faces_element = i;
				has_faces = true;
			}
		}
	}

	//testing for required vertex fields.
	if(pf.AddToRead(plyprop1[0])==-1 ||
			pf.AddToRead(plyprop1[1])==-1 ||
			pf.AddToRead(plyprop1[2])==-1) {
		
		if(pf.AddToRead(doublecoords[0])==-1 ||
				pf.AddToRead(doublecoords[1])==-1 ||
				pf.AddToRead(doublecoords[2])==-1) {
			throw QString("ply file is missing xyz coords");
		} else {
			double_coords = true;
		}
	}

	//these calls will silently fail if no color is present
	int error = pf.AddToRead(plyprop1[3]);
	pf.AddToRead(plyprop1[4]);
	pf.AddToRead(plyprop1[5]);
	pf.AddToRead(plyprop1[6]);

	if(error ==  vcg::ply::E_NOERROR)
		has_colors = true;
	else {
		error = pf.AddToRead(plyprop1[10]);
		pf.AddToRead(plyprop1[11]);
		pf.AddToRead(plyprop1[12]);

		if(error ==  vcg::ply::E_NOERROR)
			has_colors = true;
	}

	//these calls will fail silently if no normal is present
	if(!has_faces) { //skip normals for triangle mesh
		error = pf.AddToRead(plyprop1[7]);
		pf.AddToRead(plyprop1[8]);
		pf.AddToRead(plyprop1[9]);
		if(error == vcg::ply::E_NOERROR)
			has_normals = true;
	}

	pf.AddToRead(vindex[0]);
	pf.AddToRead(vindex_uint[0]);
	pf.AddToRead(plyprop3[0]);
	pf.AddToRead(plyprop3_uint[0]);

	//these calls will fail silently if no texture is present
	if (pf.AddToRead(plyprop4[0]) == vcg::ply::E_NOERROR)
		has_textures = true;

	if (pf.AddToRead(plyprop5[0]) == vcg::ply::E_NOERROR) { }

	pf.SetCurElement(vertices_element);
}

void PlyLoader::cacheVertices() {
	vertices.setElementsPerBlock(1<<20);
	vertices.resize(n_vertices);

	PlyVertex vertex;
	//caching vertices on temporary file
	for(quint64 i = 0; i < n_vertices; i++) {
		Vertex &v = vertices[i];
		pf.Read((void *)&vertex);
		if(double_coords) {
			v.v[0] = vertex.dv[0] - origin[0];
			v.v[1] = vertex.dv[1] - origin[1];
			v.v[2] = vertex.dv[2] - origin[2];
		} else {
			v.v[0] = vertex.v[0] - origin[0];
			v.v[1] = vertex.v[1] - origin[1];
			v.v[2] = vertex.v[2] - origin[2];
		}
		if(has_colors) {
			v.c[0] = vertex.c[0];
			v.c[1] = vertex.c[1];
			v.c[2] = vertex.c[2];
			v.c[3] = vertex.c[3];
		}
		if(has_textures) {
			v.t[0] = vertex.t[0];
			v.t[1] = vertex.t[1];
		}
		
		if(quantization) {
			quantize(v.v[0]);
			quantize(v.v[1]);
			quantize(v.v[2]);
		}
	}

	pf.SetCurElement(faces_element);
}

void PlyLoader::setMaxMemory(quint64 max_memory) {
	vertices.setMaxMemory(max_memory);
}

quint32 PlyLoader::getTriangles(quint32 size, Triangle *buffer) {
	if(faces_element == -1)
		throw QString("ply has no faces!");

	if(current_triangle == 0)
		cacheVertices();

	if(current_triangle >= n_triangles) return 0;

	quint32 count = 0;

	PlyFace face;
	face.texNumber = 0;
	for(quint32 i = 0; i < size && current_triangle < n_triangles; i++) {

		pf.Read((void *) &face);
		Triangle &current = buffer[count];
		for(int k = 0; k < 3; k++) {
			Vertex &vertex = vertices[face.f[k]];
			vertex.t[0] = face.t[k*2];
			vertex.t[1] = face.t[k*2+1];

			current.vertices[k] = vertex;
		}
		//TODO! detect complicated texture coordinates
		
		current.node = 0;
		current.tex = face.texNumber + texOffset;

		current_triangle++;

		//ignore degenerate triangles
		if(current.isDegenerate())
			continue;

		count++;
	}
	return count;
}

quint32 PlyLoader::getVertices(quint32 size, Splat *splats) {
	if(current_triangle > n_triangles) return 0;

	PlyVertex vertex;
	
	Splat v;
	v.node = 0;
	v.c[3] = 255;
	quint32 count = 0;
	for(quint32 i = 0; i < size && current_vertex < n_vertices; i++) {

		pf.Read((void *)&vertex);

		Splat &v = splats[count++];
		current_vertex++;

		if(double_coords) {
			box.Add(vcg::Point3d(vertex.dv) - origin);
			v.v[0] = (float)(vertex.dv[0] - origin[0]);
			v.v[1] = (float)(vertex.dv[1] - origin[1]);
			v.v[2] = (float)(vertex.dv[2] - origin[2]);
		} else {
			box.Add(vcg::Point3d(vertex.v[0], vertex.v[1], vertex.v[2]) - origin);
			v.v[0] = vertex.v[0] - (float)origin[0];
			v.v[1] = vertex.v[1] - (float)origin[1];
			v.v[2] = vertex.v[2] - (float)origin[2];
		}
		if(has_colors) {
			v.c[0] = vertex.c[0];
			v.c[1] = vertex.c[1];
			v.c[2] = vertex.c[2];
			v.c[3] = vertex.c[3];
		}
		if(has_textures) {
			v.t[0] = vertex.t[0];
			v.t[1] = vertex.t[1];
		}
		if(has_normals) {
			v.n[0] = vertex.n[0];
			v.n[1] = vertex.n[1];
			v.n[2] = vertex.n[2];
		}

		if(quantization) {
			quantize(v.v[0]);
			quantize(v.v[1]);
			quantize(v.v[2]);
		}
	}
	return count;
}
