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
#include <assert.h>
#include "virtualarray.h"
#include <iostream>

#include <QDir>

using namespace std;

VirtualMemory::VirtualMemory(QString prefix):
	QTemporaryFile(QDir::tempPath() +"/" + prefix),
	used_memory(0),
	max_memory(1<<28) {

	setAutoRemove(true);
	if(!open())
		throw QString("unable to open temporary file: " + QDir::tempPath() +"/" + prefix);

}

VirtualMemory::~VirtualMemory() {
	flush();
}

void VirtualMemory::setMaxMemory(quint64 n) {
	max_memory = n;
}

uchar *VirtualMemory::getBlock(quint64 index, bool prevent_unload) {

	assert(index < cache.size());
	if(cache[index] == NULL) { //not mapped.
		if(!prevent_unload)
			makeRoom();
		mapBlock(index);
		if(!cache[index])
			throw QString("virtual memory error mapping block: " + this->errorString());

		mapped.push_front(index);
	}
	return cache[index];
}

void VirtualMemory::dropBlock(quint64 index) {
	unmapBlock(index);
}

void VirtualMemory::resize(quint64 n, quint64 n_blocks) {
#ifndef WIN32
	if(n < (quint64)size())
		flush();
#else
	flush();
#endif
	cache.resize(n_blocks, NULL);
	QTemporaryFile::resize(n);
#ifdef WIN32
	/*for (qint64 i = 0; i < cache.size(); i++)
		mapBlock(i);*/
#endif
}

quint64 VirtualMemory::addBlock(quint64 length) {
#ifdef WIN32
	flush();
#endif
	cache.push_back(NULL);
	QFile::resize(size() + length);
#ifdef WIN32
	/*for (qint64 i = 0; i < cache.size(); i++)
		mapBlock(i);*/
#endif
	return cache.size()-1;
}

void VirtualMemory::flush() {
	for(quint32 i = 0; i < cache.size(); i++) {
		if(cache[i])
			unmapBlock(i);
	}
	mapped.clear();
	used_memory = 0;
}

void VirtualMemory::makeRoom() {
	while(used_memory > max_memory) {
		assert(mapped.size());
		quint64 block = mapped.back();
		if(cache[block])
			unmapBlock(block);
		mapped.pop_back();
	}
}

uchar *VirtualMemory::mapBlock(quint64 block) {
	quint64 offset = blockOffset(block);
	quint64 length = blockSize(block);
	assert(offset + length <= (quint64)QFile::size());
	cache[block] = map(offset, length);
	used_memory += length;
	return cache[block];
}

void VirtualMemory::unmapBlock(quint64 block) {
	assert(block < cache.size());
	assert(cache[block]);
	unmap(cache[block]);
	cache[block] = NULL;
	used_memory -= blockSize(block);
}


