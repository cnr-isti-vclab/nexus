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
#ifndef NX_PLYLOADER_H
#define NX_PLYLOADER_H

#include "meshloader.h"
#include "../common/virtualarray.h"


#include <wrap/ply/plylib.h>


class PlyLoader: public MeshLoader {
public:
	PlyLoader(QString file);
	~PlyLoader();

	void setMaxMemory(quint64 max_memory);
	quint32 getTriangles(quint32 size, Triangle *buffer);
	quint32 getVertices(quint32 size, Splat *vertex);

	quint32 nVertices() { return n_vertices; }
	quint32 nTriangles() { return n_triangles; }
private:
	vcg::ply::PlyFile pf;
	bool double_coords = false;
	bool has_vertex_tex_coords = false;
	qint64 vertices_element;
	qint64 faces_element;

	VirtualArray<Vertex> vertices;
	quint64 n_vertices;
	quint64 n_triangles;
	quint64 current_triangle;
	quint64 current_vertex;

	void init();
	void cacheVertices();
};

#endif // NX_PLYLOADER_H

