#ifndef EXTRACTOR_H
#define EXTRACTOR_H

#include <QFile>

#include <vcg/math/matrix44.h>

#include "../common/traversal.h"

class Extractor: public nx::Traversal {
public:
	//compression options
	int coord_q;      //step of coord quantizations (a power of 2: 0 means 1 unit)
	double error_factor; //unused error for internal vertices
	int color_bits[4];
	int norm_bits;
	float tex_step; //in pixel units

	Extractor(nx::NexusData *nexus);

	int levelCount(); //return number of levels in the nexus (assuming it's a constant depth tree)
	void setMatrix(vcg::Matrix44f m);
	void selectBySize(quint64 size);
	void selectByLevel(int level); //select up to level included (zero based)
	void selectByError(float error);
	void selectByTriangles(quint64 triangles);
	void dropLevel();

	void save(QString output, nx::Signature &signature);
	void savePly(QString filename);
	void saveStl(QString filename);

	void saveUnifiedPly(QString filename);

	virtual float nodeError(uint32_t node, bool &visible);
	nx::Traversal::Action expand(HeapNode h);

protected:

	bool transform;
	vcg::Matrix44f matrix;
	quint64 max_size, current_size;
	float min_error, current_error;
	int max_level = -1, nlevels = 0;
	quint64 max_triangles, current_triangles;

	quint32 pad(quint32 s);
	int sinkDistance(int node);

	void compress(QFile &file, nx::Signature &signature, nx::Node &node, nx::NodeData &data, nx::Patch *patches);
	//counts vertices and faces in the selected mesh.
	void countElements(quint64 &n_vertices, quint64 &n_faces);
};

#endif // EXTRACTOR_H
