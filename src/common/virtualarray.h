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
#ifndef NX_VIRTUALARRAY_H
#define NX_VIRTUALARRAY_H

#include <assert.h>

#include <QTemporaryFile>

#include <vector>
#include <deque>
#include <iostream>

using namespace std;
/*
  blocks are 64 bit memory aligned!
*/

class VirtualMemory: public QTemporaryFile {
public:
	VirtualMemory(QString prefix);
	virtual ~VirtualMemory();

	quint64 memoryUsed() { return used_memory; }
	quint64 maxMemory() { return max_memory; }
	void setMaxMemory(quint64 max_memory);

	//careful: if unload is false, memory is valid until another call to a function of this classe
	uchar *getBlock(quint64 block, bool unload = false);
	void dropBlock(quint64 block);
	quint64 addBlock(quint64 length);         //return index of added block

	quint64 nBlocks() { return cache.size(); }
	void resize(quint64 size, quint64 n_blocks);
	void flush();

protected:

	virtual quint64 blockOffset(quint64 block) = 0;
	virtual quint64 blockSize(quint64 block) = 0;

	uchar *mapBlock(quint64 block);
	void unmapBlock(quint64 block);
	void makeRoom();

private:
	quint64 used_memory;
	quint64 max_memory;
	std::vector<uchar *> cache;   //1 pointer per block Nu
	std::deque<quint64> mapped;
};


template<class ITEM> class VirtualArray: public VirtualMemory {
public:
	VirtualArray(QString prefix): VirtualMemory(prefix), n_elements(0), elements_per_block(1<<16) {
		block_size = elements_per_block * sizeof(ITEM);
	}
	~VirtualArray() { flush(); }

	void setElementsPerBlock(quint64 n) {
		elements_per_block = n;
		block_size = elements_per_block * sizeof(ITEM);
	}
	quint64 size() {
		return n_elements;
	}
	void resize(quint64 s) {   //number of elements, not memory
		//need to round up s
		quint64 blocks = (s + elements_per_block -1) / elements_per_block;
		n_elements = s;
		if(blocks != VirtualMemory::nBlocks())
			VirtualMemory::resize(blocks*block_size, blocks);
	}
	ITEM &operator[](quint64 n) {
		//find block
		quint64 block = n / elements_per_block;
		quint64 offset = n - block * elements_per_block;
		uchar *buffer = getBlock(block);
		return *(ITEM *)(buffer + offset * sizeof(ITEM));
	}

protected:
	quint64 n_elements;           //number of elements
	quint64 elements_per_block;   //default 1<<16
	quint64 block_size;           //in bytes

	quint64 blockOffset(quint64 block) { return block * block_size; }
	quint64 blockSize(quint64 /*block*/) { return block_size; }
};

class VirtualChunks: public VirtualMemory {
public:
	VirtualChunks(QString prefix): VirtualMemory(prefix), padding(64) {
		offsets.push_back(0);
	}
	~VirtualChunks() { flush(); }
	void setPadding(quint32 p) { padding = p; }

	quint64 addChunk(quint64 size) {
		//pad size:
		size = pad(size);
		offsets.push_back(offsets.back() + size);
		addBlock(size);
		return offsets.size() -2;
	}
	uchar *getChunk(quint64 chunk, bool unload = false) { return getBlock(chunk, unload); }
	void dropChunk(quint64 chunk) { unmapBlock(chunk); }

	quint64 chunkSize(quint64 chunk) { return blockSize(chunk); }
protected:
	quint32 padding; //must be a power of 2
	std::vector<quint64> offsets;

	quint64 blockOffset(quint64 block) { return offsets[block]; }
	quint64 blockSize(quint64 block) { return offsets[block+1] - offsets[block]; }

	quint64 pad(quint64 size) {
		assert(size != 0);
		quint64 m = (size-1) & ~(padding -1);
		return m + padding;
	}
};

#endif // NX_VIRTUALARRAY_H
