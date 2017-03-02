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
#ifndef NX_BYTESTREAM_H
#define NX_BYTESTREAM_H

#include <assert.h>

#include <vector>
#include <iostream>
using namespace std;

//input and output byte: you must know when to stop reading.

class OutputStream: public std::vector<unsigned char> {
public:
	OutputStream(): pos(0) {}
	void writeByte(unsigned char byte) { push_back(byte); }
	template<class T> void write(T &t) {
		int s = size();
		resize(s + sizeof(T));
		*(T *)(&*begin() + s) = t;
	}
	unsigned char readByte() { return operator[](pos++); }
	void restart() { pos = 0; }

	int pos;
private:

};

//read and write into memory directly, ensure there is enough space

class InputStream {
public:
	unsigned char *mem; //current memory location
	int count;
	int max_length;

	InputStream(unsigned char *m, int maxlen): mem(m), count(0), max_length(maxlen) {}
	unsigned char readByte() { assert(count < max_length); return mem[count++]; }
	template<class T> T read() {
		T &t = *(T *)(mem + count);
		count += sizeof(T);
		return t;
	}
	void restart() { count = 0; }
};

#endif // NX_BYTESTREAM_H
