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
	if(ascii)
		throw QString("Unsupported ascii STL");

	header = file.read(80); //ignored
	file.read((char *)&n_triangles, 4); //TODO support little endian in big-endiam systems

}

STLLoader::~STLLoader() {

}


quint32 STLLoader::getTriangles(quint32 size, Triangle *buffer) {
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
				tri.vertices[t].v[k] = pos[t*3 + k];
		tri.node = 0;
		current_triangle++;
		start += 50;
	}
	return nread;
}

quint32 STLLoader::getVertices(quint32 /*size*/, Splat */*vertex*/) {
	throw QString("Unimplemented!");
}
