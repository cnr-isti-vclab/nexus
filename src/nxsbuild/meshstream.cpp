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
#include <QDebug>
#include <QFileInfo>

#include "meshstream.h"
#include "meshloader.h"
#include "plyloader.h"
#include "tsploader.h"
#include "objloader.h"
#include "stlloader.h"
#include "vcgloadermesh.h"
#include "vcgloader.h"


#include <iostream>

using namespace std;

Stream::Stream():
	has_colors(false),
	has_normals(false),
	has_textures(false),
	vertex_quantization(0),
	current_triangle(0),
	current_block(0) {
}

void Stream::setVertexQuantization(double q) {
	vertex_quantization = q;
}

MeshLoader *Stream::getLoader(QString file, QString material) {
MeshLoader *loader = nullptr;
	if(file.endsWith(".ply"))
		loader = new PlyLoader(file);

	else if(file.endsWith(".tsp"))
		loader = new TspLoader(file);

	else if(file.endsWith(".obj"))
		loader = new ObjLoader(file, material);

	else if(file.endsWith(".stl"))
		loader = new STLLoader(file);

	else
		loader = new VcgLoader<VcgMesh>(file);
	/*        else if(file.endsWith(".off"))
		loader = new OffLoader(file, vertex_quantization, max_memory);*/

//	else
//		throw QString("Input format for file " + file + " is not supported at the moment");
	return loader;
}

vcg::Box3d Stream::getBox(QStringList paths) {

	vcg::Box3d box;

	quint32 length = (1<<20); //times 52 bytes. (128Mb of data)
	Splat *vertices = new Splat[length];
	
	
	foreach(QString file, paths) {
		qDebug() << "Computing box for " << qPrintable(file);
		MeshLoader *loader = getLoader(file, QString());
		loader->setMaxMemory(512*(1<<20)); //read only once... does'nt really matter.
		while(true) {
			int count = loader->getVertices(length, vertices);
			if(count == 0) break;
		}
		box.Add(loader->box);
		delete loader;
	}
	delete []vertices;
	return box;
}

void Stream::load(MeshLoader *loader) {
	loader->setVertexQuantization(vertex_quantization);
	loader->origin = origin;
	loadMesh(loader);
	has_colors &= loader->hasColors();
	has_normals &= loader->hasNormals();
	has_textures &= loader->hasTextures();

	if(has_textures) {
		for(auto tex: loader->texture_filenames) {
			textures.push_back(tex);
		}
	}
}

void Stream::load(QStringList paths, QString material) {

	has_colors = true;
	has_normals = true;
	has_textures = true;
	foreach(QString file, paths) {
		qDebug() << "Reading" << qPrintable(file);
		MeshLoader *loader = getLoader(file, material);
		load(loader);
		//box.Add(loader->box); //this lineB AFTER the mesh is streamed
		delete loader;
	}
	current_triangle = 0;
	flush();
}


void Stream::clear() {
	clearVirtual();
	levels.clear();
	order.clear();
	textures.clear();
	current_triangle = 0;
	current_block = 0;
	box = vcg::Box3f();
}

//bit twiddling hacks
quint64 Stream::getLevel(qint64 v) {
	static const int MultiplyDeBruijnBitPosition[32] =  {
		0, 1, 28, 2, 29, 14, 24, 3, 30, 22, 20, 15, 25, 17, 4, 8,
		31, 27, 13, 23, 21, 19, 16, 7, 26, 12, 18, 6, 11, 5, 10, 9
	};
	return MultiplyDeBruijnBitPosition[((unsigned int)((v & -v) * 0x077CB531U)) >> 27];
}

void Stream::computeOrder() {
	order.clear();
	for(int level = levels.size()-1; level >= 0; level--) {
		std::vector<quint64> &level_blocks = levels[level];
		for(uint i = 0; i < level_blocks.size(); i++) {
			order.push_back(level_blocks[i]);
		}
	}
}



//SOUP

StreamSoup::StreamSoup(QString prefix):
	VirtualTriangleSoup(prefix) {
}

void StreamSoup::loadMesh(MeshLoader *loader) {
	loader->setMaxMemory(maxMemory());
	loader->texOffset = textures.size();
	//get 128Mb of data
	quint32 length = (1<<20); //times 52 bytes.
	Triangle *triangles = new Triangle[length];
	while(true) {
		int count = loader->getTriangles(length, triangles);
		if(count == 0) break;
		for(int i = 0; i < count; i++) {
			assert(triangles[i].node == 0);
			pushTriangle(triangles[i]);
		}
	}
	delete []triangles;
}

void StreamSoup::pushTriangle(Triangle &triangle) {

	//ignore degenerate faces
	vcg::Point3f v[3];
	for(int k = 0; k < 3; k++) {
		v[k] = vcg::Point3f(triangle.vertices[k].v);
		box.Add(v[k]);
	}

	/* MOVED TO LOADER AND SIMPLIFIER */
	/*ignore degenerate faces
	if(v[0] == v[1] || v[1] == v[2] || v[2] == v[0])
		return; */

	quint64 level = getLevel(current_triangle);
	assert(levels.size() >= level);

	quint64 block = 0;
	if(levels.size() == level) {  //need to add a level
		levels.push_back(std::vector<quint64>());
		block = addBlock(level);

	} else {
		block = levels[level].back();
		if(isBlockFull(block))
			block = addBlock(level);
	}

	Soup soup = get(block);
	soup.push_back(triangle);

	current_triangle++;
}

Soup StreamSoup::streamTriangles() {
	if(current_block == 0)
		computeOrder();
	if(current_block == order.size())
		return Soup(NULL, NULL, 0);

	flush(); //save memory
	quint64 block = order[current_block];
	current_block++;

	return get(block);
}

void StreamSoup::clearVirtual() {
	VirtualTriangleSoup::clear();
}

quint64 StreamSoup::addBlock(quint64 level) {
	quint64 block = VirtualTriangleSoup::addBlock();
	levels[level].push_back(block);
	return block;
}



//Cloud

StreamCloud::StreamCloud(QString prefix):
	VirtualVertexCloud(prefix) {
}

void StreamCloud::loadMesh(MeshLoader *loader) {
	loader->setMaxMemory(maxMemory());
	//get 128Mb of data
	quint32 length = (1<<20); //times 52 bytes.
	Splat *vertices = new Splat[length];
	while(true) {
		int count = loader->getVertices(length, vertices);

		if(count == 0) break;
		for(int i = 0; i < count; i++) {
			assert(vertices[i].node == 0);
			pushVertex(vertices[i]);
		}
	}
	delete []vertices;
}

void StreamCloud::pushVertex(Splat &vertex) {

	//ignore degenerate faces
	vcg::Point3f p(vertex.v);
	box.Add(p);

	quint64 level = getLevel(current_triangle);
	assert(levels.size() >= level);

	quint64 block = 0;
	if(levels.size() == level) {  //need to add a level
		levels.push_back(std::vector<quint64>());
		block = addBlock(level);

	} else {
		block = levels[level].back();
		if(isBlockFull(block))
			block = addBlock(level);
	}

	Cloud cloud = get(block);
	cloud.push_back(vertex);

	current_triangle++;
}

Cloud StreamCloud::streamVertices() {
	if(current_block == 0)
		computeOrder();

	if(current_block == order.size())
		return Cloud(NULL, NULL, 0);

	flush(); //save memory
	quint64 block = order[current_block];
	current_block++;

	return get(block);
}

void StreamCloud::clearVirtual() {
	VirtualVertexCloud::clear();
}

quint64 StreamCloud::addBlock(quint64 level) {
	quint64 block = VirtualVertexCloud::addBlock();
	levels[level].push_back(block);
	return block;
}

