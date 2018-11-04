#ifndef STLLOADER_H
#define STLLOADER_H

#include "meshloader.h"

class STLLoader: public MeshLoader {
public:
	STLLoader(QString filename);
	~STLLoader();

	void setMaxMemory(quint64 /*max_memory*/) { /* ignore, here no memory needed */ }
	quint32 getTriangles(quint32 size, Triangle *buffer);
	quint32 getVertices(quint32 size, Splat *vertex);

	quint32 nVertices() { return n_vertices; }
	quint32 nTriangles() { return n_triangles; }
private:
	QFile file;

	bool ascii;
	quint64 n_vertices;
	quint64 n_triangles;
	quint64 current_triangle;
	quint64 current_vertex;
};

#endif // STLLOADER_H

