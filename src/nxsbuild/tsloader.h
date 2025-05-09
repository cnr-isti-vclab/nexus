#ifndef TSLOADER_H
#define TSLOADER_H

#include "meshloader.h"
#include "colormap.h"

class TsLoader: public MeshLoader {
public:
	TsLoader(QString file);
	~TsLoader();

	bool useColormapFor(const QString &_property, const QString &palette);
	void setMaxMemory(quint64 max_memory);
	quint32 getTriangles(quint32 size, Triangle *buffer);
	quint32 getVertices(quint32 size, Splat *vertex);

private:

	void cacheVertices();

	QFile file;
	VirtualArray<Vertex> vertices;
	quint64 n_vertices;
	quint64 n_triangles;
	quint64 current_triangle;
	quint64 current_vertex;
	qint64  current_tri_pos = 0;
	QString property;
	int property_position = -1;
	Colormap colormap;
};
#endif // TSLOADER_H
