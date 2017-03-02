#ifndef EXTRACTOR_H
#define EXTRACTOR_H

#include <QFile>

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

	void setMatrix(vcg::Matrix44f m);
	void selectBySize(quint64 size);
	void selectByError(float error);
	void selectByTriangles(quint64 triangles);
	void dropLevel();

	void save(QString output, nx::Signature &signature);
	void savePly(QString filename);

	void saveUnifiedPly(QString filename);
protected:

	bool transform;
	vcg::Matrix44f matrix;
	quint64 max_size, current_size;
	float min_error, current_error;
	quint64 max_triangles, current_triangles;



	Action expand(HeapNode h);
	quint32 pad(quint32 s);

	void compress(QFile &file, nx::Signature &signature, nx::Node &node, nx::NodeData &data, nx::Patch *patches);
};

#endif // EXTRACTOR_H
