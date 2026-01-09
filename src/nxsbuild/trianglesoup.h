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
#ifndef NX_TRIANGLESOUP_H
#define NX_TRIANGLESOUP_H

#include <vcg/space/point3.h>
#include "../common/virtualarray.h"

//16  bytes
struct Vertex {
	float v[3];
	unsigned char c[4]; //colors
	float t[2]; //texture

	bool operator==(const Vertex &p) const {
		return v[0] == p.v[0] && v[1] == p.v[1] && v[2] == p.v[2];
	}
};

struct Splat: public Vertex {
	quint32 node;
	float n[3];
};

//52 bytes.
struct Triangle {
	Vertex vertices[3];
	quint32 node;
	int tex;  //which tex this triangle refers to.
	bool isDegenerate() const {
		if(vertices[0] == vertices[1] || vertices[0] == vertices[2] || vertices[1] == vertices[2])
			return true;
		return false;
	}
};

template <class T> class Bin {
public:
	T *elements;
	quint32 *_size;
	quint32 capacity;

	Bin(): elements(0), _size(0), capacity(0) {}
	Bin(T *start, quint32 *s, quint32 c):
		elements(start), _size(s), capacity(c) {}

	quint32 size() { if(!_size) return 0; return *_size; }
	void resize(quint32 s) { *_size = s; }
	bool isFull() { return *_size == capacity; }
	T &operator[](uint n) {
		assert(n < *_size);
		return elements[n];
	}

	void push_back(T &element) {
		assert(!isFull());
		elements[*_size] = element;
		(*_size)++;
	}
};

typedef Bin<Triangle> Soup;
typedef Bin<Splat> Cloud;

template <class T> class VirtualBin: protected VirtualMemory {
public:

	//Triangle soup is guaranteed valid only until another call of getSoup (or resize, or clear)
	//unless prevent_unload is true

	VirtualBin(QString prefix):
		VirtualMemory(prefix),
		triangles_per_block(1<<15),
		block_size((1<<15) * sizeof(Triangle)) {
	}
	~VirtualBin() { flush(); }

	quint64 memoryUsed() { return VirtualMemory::memoryUsed(); }
	void setMaxMemory(quint64 m) { VirtualMemory::setMaxMemory(m); }
	quint64 maxMemory() { return VirtualMemory::maxMemory(); }

	Bin<T> get(quint64 n, bool prevent_unload = false) {
		uchar *memory = getBlock(n, prevent_unload);
		return Bin<T>((T *)memory, &occupancy[n], triangles_per_block);
	}

	void drop(quint64 n) {
		unmapBlock(n);
	}

	quint64 size() {
		quint64 tot = 0;
		for(uint i = 0; i < occupancy.size(); i++)
			tot += occupancy[i];
		return tot;
	}

	void clear() {
		resize(0, 0);
		occupancy.clear();
	}

	void setTrianglesPerBlock(quint64 s) {
		triangles_per_block = s;
		block_size = triangles_per_block * sizeof(T);
	}

	quint64 addBlock() {
		assert(occupancy.size() == nBlocks());
		quint64 block = VirtualMemory::addBlock(block_size);
		occupancy.push_back(0);
		return block;
	}
	quint64 nBlocks() { return VirtualMemory::nBlocks(); }

	bool isBlockFull(quint64 block) {
		return occupancy[block] == triangles_per_block;
	}

	quint32 blockUsed(quint64 block) {
		return occupancy[block];
	}

	/*
	Soup getSoup(quint64 n, bool prevent_unload = false);
	void dropSoup(quint64 n);

	quint64 size();
	void clear();
	void setTrianglesPerBlock(quint64 s);
	quint64 addBlock();

	bool isBlockFull(quint64 block); */

protected:
	quint64 triangles_per_block;
	quint64 block_size;
	std::vector<quint32> occupancy;      //how many triangles per block

	virtual quint64 blockOffset(quint64 block) { return block * block_size; }
	virtual quint64 blockSize(quint64 /*block*/) { return block_size; }
};

typedef VirtualBin<Triangle> VirtualTriangleSoup;
typedef VirtualBin<Splat> VirtualVertexCloud;


#endif // NX_TRIANGLESOUP_H
