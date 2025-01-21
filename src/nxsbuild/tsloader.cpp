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
#include "tsloader.h"
#include <QFileInfo>
#include <QDir>
#include <QTextStream>
#include <iostream>


TsLoader::TsLoader(QString filename):
	vertices("cache_tsvertex"),
	n_vertices(0),
	n_triangles(0),
	current_vertex(0) {

	file.setFileName(filename);
	if(!file.open(QFile::ReadOnly))
		throw QString("could not open file %1. Error: %2").arg(filename).arg(file.errorString());
}

TsLoader::~TsLoader() {
	file.close();
}



void TsLoader::cacheVertices() {
	vertices.setElementsPerBlock(1<<20);
	file.seek(0);
	char buffer[1024];
	char tmp[32];
	int cnt = 0;

	while(1) {

		int s = file.readLine(buffer, 1024);
		if (s == -1) {                     //end of file
			std::cout << "Vertices read: " << cnt << std::endl;
			break;

		}
		if(s == 0) continue;            //skip empty lines
		buffer[s] = '\0';               //terminating line, readLine wont do this.

		if(strncmp(buffer, "VRTX", 4) == 0 || strncmp(buffer, "PVRTX", 5) == 0) {

			vertices.resize(n_vertices+1);
			Vertex &vertex = vertices[n_vertices];
			n_vertices++;

			vcg::Point3d p;
			int id;
			int n = sscanf(buffer, "%*s %*d %lf %lf %lf", &p[0], &p[1], &p[2]);
			if(n != 3) throw QString("error parsing vertex line %1 while caching").arg(buffer);
			p -= origin;
			p[0] *= scale[0];
			p[1] *= scale[1];
			p[2] *= scale[2];
			box.Add(p);

			vertex.v[0] = (float)p[0];
			vertex.v[1] = (float)p[1];
			vertex.v[2] = (float)p[2];

			cnt++;
			if(quantization) {
				quantize(vertex.v[0]);
				quantize(vertex.v[1]);
				quantize(vertex.v[2]);
			}
		}
		if(strncmp(buffer, "ATOM", 4) == 0) {
			int id;
			int n = sscanf(buffer, "%*s %*d %d", &id);
			if(n != 1) throw QString("error parsing atom line %1 while caching").arg(buffer);
			vertices.resize(n_vertices+1);
			vertices[n_vertices] = vertices[id -1];
		}
	}
}

void TsLoader::setMaxMemory(quint64 max_memory) {
	vertices.setMaxMemory(max_memory);
}

quint32 TsLoader::getTriangles(quint32 size, Triangle *faces) {

	if (n_triangles == 0) {
		cacheVertices();
	}

	char buffer[1024];
	file.seek(current_tri_pos);

	qint32 R = (0x7f << 24);
	qint32 G = (0x7f << 16);
	qint32 B = (0x7f << 8);

	quint32 count = 0;
	qint64 cpos = current_tri_pos;

	while (count < size) {
		cpos = file.pos();
		int s = file.readLine(buffer, 1024);
		if (s == -1) {                     //end of file
			cpos = file.pos();
			break;
		}

		if (strncmp(buffer, "TRGL", 4) != 0)
			continue;

		Triangle &current = faces[count];
		int f[3];
		int n = sscanf(buffer, "%*s %d %d %d", f, f+1, f+2);
		if(n != 3) throw QString("error parsing triangle line %1 while reading").arg(buffer);

		current.vertices[0] = vertices[f[0]-1];
		current.vertices[1] = vertices[f[1]-1];
		current.vertices[2] = vertices[f[2]-1];

		current.node = 0;
		count++;
		n_triangles++;
		cpos = file.pos();
	}

	current_tri_pos = cpos;

	if (count == 0)
		std::cout << "faces read: " << n_triangles << std::endl;

	return count;
}


/*
 * Seeks all verteces in whole file
 */
quint32 TsLoader::getVertices(quint32 size, Splat *vertices) {
	char buffer[1024];

	quint32 count = 0;
	while(count < size) {
		int s = file.readLine(buffer, 1024);
		if(s == -1)                     //end of file
			return count;

		if(strncmp(buffer, "VRTX", 4) != 0 || strncmp(buffer, "PVRTX", 5) != 0)
			continue;


		buffer[s] = '\0';               //terminating line, readLine wont do this.


		Splat &vertex = vertices[count];

		vcg::Point3d p;
		int n = sscanf(buffer, "%*s %*d %lf %lf %lf", &p[0], &p[1], &p[2]);
		if(n != 3) throw QString("error parsing vertex line %1").arg(buffer);

		p -= origin;
		p[0] *= scale[0];
		p[1] *= scale[1];
		p[2] *= scale[2];
		box.Add(p);

		vertex.v[0] = (float)p[0];
		vertex.v[1] = (float)p[1];
		vertex.v[2] = (float)p[2];

		if(quantization) {
			quantize(vertex.v[0]);
			quantize(vertex.v[1]);
			quantize(vertex.v[2]);
		}
		n_vertices++;
		current_vertex++;
		count++;
	}
	return count;
}
