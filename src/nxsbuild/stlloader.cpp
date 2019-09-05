#include "stlloader.h"

#include <iostream>
using namespace std;

STLLoader::STLLoader(QString filename) {

	has_colors = has_normals = has_textures = false;

	file.setFileName(filename);
	if(!file.open(QIODevice::ReadOnly))
		throw QString("could not open file " + filename);

	QByteArray header = file.peek(5);
	if(header.size() != 5)
		throw QString("Unexpected end of file");
	ascii = header.startsWith("solid");
	if(ascii) {
		file.readLine();
		n_triangles = 0;
		return;
	}
	header = file.read(80); //ignored
	file.read((char *)&n_triangles, 4); //TODO support little endian in big-endiam systems

}


/*
facet normal nx ny nz
	outer loop
		vertex v1x v1y v1z
		vertex v2x v2y v2z
		vertex v3x v3y v3z
	endloop
endfacet */

quint32 STLLoader::getTrianglesAscii(quint32 size, Triangle *buffer) {
	quint32 readed = 0;
	char line[1024];
	char dummy[1024];
	
	for(int i = 0; i < size; i++) {

		qint64 ok = file.readLine(line, 1023); //facet
		if(ok <= 0) return readed;

		ok = file.readLine(line, 1023); //normal
		if(ok <= 0) return readed;


		Triangle &tri = buffer[i];
		tri.node = 0;

		for(int k = 0; k < 3; k++) {
			float *v = tri.vertices[k].v;
			ok = file.readLine(line, 1023); //normal
			if(ok <= 0) return readed;

			vcg::Point3d d;
			int n = sscanf(line, "%s %lf %lf %lf", dummy, &d[0], &d[1], &d[2]);
			if(n != 4)
				throw QString("Invalid STL file");
			d -= origin;
			box.Add(d);
					
			v[0] = (float)(d[0]);
			v[1] = (float)(d[1]);
			v[2] = (float)(d[2]);
		}
		current_triangle++;
		readed++;

		ok = file.readLine(line, 1023); //endloop
		if(ok <= 0) return readed;

		ok = file.readLine(line, 1023); //endfacet
		if(ok <= 0) return readed;
	}
	return readed;
}

quint32 STLLoader::getTrianglesBinary(quint32 size, Triangle *buffer) {
	//each face is normal (float), + 3 vertices(float) + uint16  = 50 bytes.
	vector<char> tmp(50*size);
	qint64 nread = file.read(tmp.data(), 50*size);

	nread /= 50;
	if(nread <= 0) return 0;

	//todo check the file is not truncated!


	char *start = tmp.data();
	for(int i = 0; i < nread; i++) {
		float *pos = (float *)(start + 12); //skip normal
		Triangle &tri = buffer[i];
		for(int t = 0; t < 3; t++)
			for(int k = 0; k < 3; k++)
				tri.vertices[t].v[k] = pos[t*3 + k] - origin[k];
		tri.node = 0;
		current_triangle++;
		start += 50;
	}
	return nread;
}

quint32 STLLoader::getVertices(quint32 /*size*/, Splat */*vertex*/) {
	throw QString("Unimplemented!");
}
