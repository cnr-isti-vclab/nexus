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
#include "objloader.h"

ObjLoader::ObjLoader(QString filename):
	vertices("cache_plyvertex"),
	n_vertices(0),
	n_triangles(0),
	current_triangle(0),
	current_vertex(0) {

	file.setFileName(filename);
	if(!file.open(QFile::ReadOnly))
		throw QString("Could not open file '%1' error: %2").arg(filename).arg(file.errorString());
}

ObjLoader::~ObjLoader() {
	file.close();
}

void ObjLoader::cacheVertices() {
	vertices.setElementsPerBlock(1<<20);

	char buffer[1024];

	while(1) {
		quint64 pos = file.pos();
		int s = file.readLine(buffer, 1024);
		if(s == -1)                     //end of file
			break;
		if(s == 0) continue;            //skip empty lines
		if(buffer[0] == '#')            //skip comments
			continue;
		buffer[s] = '\0';               //terminating line, readLine wont do this.

		if(buffer[0] == 'v') {          //vertex
			if(buffer[1] == ' ') {      //skip other properties

				vertices.resize(n_vertices+1);
				Vertex &vertex = vertices[n_vertices];
				n_vertices++;

				int n = sscanf(buffer, "v %f %f %f", &(vertex.v[0]), &(vertex.v[1]), &(vertex.v[2]));
				if(n != 3) throw QString("Error parsing vertex line: %1").arg(buffer);
				if(quantization) {
					quantize(vertex.v[0]);
					quantize(vertex.v[1]);
					quantize(vertex.v[2]);
				}


			}//skipping other properties in OBJ
			continue;

		} else if(buffer[0] == 'f') {    //triangle, bail out
			file.seek(pos);
			break;
		}
	}

}

void ObjLoader::setMaxMemory(quint64 max_memory) {
	vertices.setMaxMemory(max_memory);
}

quint32 ObjLoader::getTriangles(quint32 size, Triangle *faces) {
	if(current_triangle == 0)
		cacheVertices();


	char st[3][100];
	char buffer[1024];

	quint32 count = 0;
	while(count < size) {
		int s = file.readLine(buffer, 1024);
		if(s == -1)                     //end of file
			return count;
		if(buffer[0] == '#')            //skip comments
			continue;
		if(buffer[0] != 'f')            //vertex
			throw QString("I was expecing a face, not this: %1").arg(buffer);

		int face[3];
		int normal[3];
		int res=sscanf(buffer, "f %s %s %s",st[0],st[1],st[2]);
		if(res != 3)
			throw QString("Could not parse face: %1").arg(buffer);
		for (int w = 0; w < 3; w++) {
			int n=strlen(st[w]);
			int rr[3]; rr[0]=rr[1]=rr[2]=0;
			int rri=0;
			for(int i=0; i<n; i++) {
				char c=st[w][i];
				if (c=='/') {
					if (rri<3) rri++;
				} else
					rr[rri]=rr[rri]*10+(c-'0');
			}
			face[w] = rr[0]-1;
			normal[w] = rr[1] -1;
		}


		current_triangle++;
		n_triangles++;

		Triangle &current = faces[count];
		for(int k = 0; k < 3; k++)
			current.vertices[k] = vertices[face[k]];
		current.node = 0;

		if(current.isDegenerate())
			continue;

		count++;
	}
	return count;
}

quint32 ObjLoader::getVertices(quint32 size, Splat *vertices) {
	char buffer[1024];

	quint32 count = 0;
	while(count < size) {
		int s = file.readLine(buffer, 1024);
		if(s == -1)                     //end of file
			return count;
		if(buffer[0] != 'v')            //skip comments, faces, etc
			continue;

		buffer[s] = '\0';               //terminating line, readLine wont do this.

		if(buffer[1] != ' ')
			continue; //skip other vertex properties{        //vertex coordinates

		Vertex &vertex = vertices[count++];

		int n = sscanf(buffer, "v %f %f %f", &(vertex.v[0]), &(vertex.v[1]), &(vertex.v[2]));
		if(n != 3) throw QString("Error parsing vertex line: %1").arg(buffer);
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
