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
#include "tsploader.h"

#include <iostream>
using namespace std;


TspLoader::TspLoader(QString filename) {

	file.setFileName(filename);
	if(!file.open(QIODevice::ReadOnly))
		throw "Could not open file: " + filename;
}

Triangle readTriangle(float *tmp) {
	Triangle triangle;
	uint color_offset = 3*3*2; //9 colors and normals
	for(int i = 0; i < 3; i++) {
		Vertex &vertex = triangle.vertices[i];
		for(int k = 0; k < 3; k++) {
			vertex.v[0] = tmp[i*3 + 0];
			vertex.v[1] = tmp[i*3 + 1];
			vertex.v[2] = tmp[i*3 + 2];

			vertex.c[0] = 255*tmp[color_offset + i*3 + 0];
			vertex.c[1] = 255*tmp[color_offset + i*3 + 1];
			vertex.c[2] = 255*tmp[color_offset + i*3 + 2];
			vertex.c[3] = 255;
		}
	}
	triangle.node = 0;
	return triangle;
}
quint32 TspLoader::getTriangles(quint32 triangle_no, Triangle *buffer) {

	uint vertex_size = 9* sizeof(float);
	uint triangle_size = 3*vertex_size;
	uint size = triangle_no * triangle_size;
	float *tmp = new float[size];
	quint32 readed = file.read((char *)tmp, size)/triangle_size;

	float *pos = tmp;
	int count = 0;
	for(uint i = 0; i < readed; i++) {
		Triangle &triangle = buffer[count];
		triangle = readTriangle(pos);
		pos += 3*9;
		if(triangle.isDegenerate()) continue;

		count++;
	}
	delete []tmp;

	return count;
}
