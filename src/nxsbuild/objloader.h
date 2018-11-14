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
#ifndef NX_OBJLOADER_H
#define NX_OBJLOADER_H

#include "meshloader.h"
#include "../common/virtualarray.h"

#include <QFile>
#include <QMap>
#include <vector>


class ObjLoader: public MeshLoader {
public:
	ObjLoader(QString file, QString mtl);
	~ObjLoader();

	void setMaxMemory(quint64 max_memory);
	quint32 getTriangles(quint32 size, Triangle *buffer);
	quint32 getVertices(quint32 size, Splat *vertex);

private:
	
	void readMTL();
	void cacheTextureUV();
	void cacheVertices();
	
	QFile file;
	QString mtl;
	VirtualArray<Vertex> vertices;
	std::vector<float> vtxtuv;
	quint64 n_vertices;
	quint64 n_triangles;
	quint64 current_triangle;
	quint64 current_vertex;
	qint64  current_tri_pos = 0;
	quint32  current_color = 0;
	qint32  current_texture_id = -1;
	QMap<QString, quint32> colors_map;
	QMap<QString, QString> textures_map;
	
	
};
#endif // NX_OBJLOADER_H
