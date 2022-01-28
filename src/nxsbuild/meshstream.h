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
#ifndef NX_MESHSTREAM_H
#define NX_MESHSTREAM_H

#include <QStringList>
#include <vcg/space/box3.h>

#include "trianglesoup.h"
#include "meshloader.h"

/* triangles are organized in levels, each one with half the triangle of the lower one
   we have a buffer for each level, when filled it gets written to disk.
   I need a vector keeping track of how much full each level is
   and a  vector of vectors holding which blocks belongs to which level
   */

class Stream {
public:

	vcg::Box3f box;
	bool has_colors;
	bool has_normals;
	bool has_textures;
	std::vector<LoadTexture> textures;
	vcg::Point3d origin = vcg::Point3d(0, 0, 0);

	Stream();
	virtual ~Stream() {}
	void setVertexQuantization(double q);
	vcg::Box3d getBox(QStringList paths);
	void load(QStringList paths, QString material);
	void load(MeshLoader *loader); //texture filenames must have the correct (relative or absolute) path!


	//return a block of triangles. The buffer is valid until next call to getTriangles. Return null when finished
	void clear();
	virtual quint64 size() = 0;
	virtual void setMaxMemory(quint64 m) = 0;
	virtual bool hasColors() { return has_colors; }
	virtual bool hasNormals() { return has_normals; }
	virtual bool hasTextures() { return has_textures; }

protected:
	std::vector<std::vector<quint64> > levels; //for each level the list of blocks
	std::vector<quint64> order;          //order of the block for streaming

	double vertex_quantization; //a power of 2.
	quint64 current_triangle;   //used both for loading and streaming
	quint64 current_block;

	MeshLoader *getLoader(QString file, QString material);
		
	virtual void flush() = 0;
	virtual void loadMesh(MeshLoader *loader) = 0;
	virtual void clearVirtual() = 0; //clear the virtualtrianglesoup or virtualtrianglebin
	virtual quint64 addBlock(quint64 level) = 0; //return index of block added

	quint64 getLevel(qint64 index);
	void computeOrder();
};


class StreamSoup: public Stream, public VirtualTriangleSoup {
public:
	StreamSoup(QString prefix);

	void pushTriangle(Triangle &triangle);
	//return a block of triangles. The buffer is valid until next call to getTriangles. Return null when finished
	Soup streamTriangles();
	quint64 size() { return VirtualTriangleSoup::size(); }
	void setMaxMemory(quint64 m) { return VirtualTriangleSoup::setMaxMemory(m); }

protected:    
	void flush() { VirtualTriangleSoup::flush(); }
	void loadMesh(MeshLoader *loader);
	void clearVirtual();
	quint64 addBlock(quint64 level); //return index of block added

};


class StreamCloud: public Stream, public VirtualVertexCloud {
public:
	StreamCloud(QString prefix);

	void pushVertex(Splat &ertex);
	//return a block of triangles. The buffer is valid until next call to getTriangles. Return null when finished
	Cloud streamVertices();
	quint64 size() { return VirtualVertexCloud::size(); }
	void setMaxMemory(quint64 m) { return VirtualVertexCloud::setMaxMemory(m); }

protected:
	void flush() { VirtualVertexCloud::flush(); }
	void loadMesh(MeshLoader *loader);
	void clearVirtual();
	quint64 addBlock(quint64 level); //return index of block added

};

#endif // NX_MESHSTREAM_H
