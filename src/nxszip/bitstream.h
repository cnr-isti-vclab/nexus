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
#ifndef NX_BITSTREAM_H
#define NX_BITSTREAM_H

#include <vector>
#include <stdint.h>

namespace meco {

class BitStream {
public:
	BitStream(): size(0), buffer(0), allocated(0), pos(0), buff(0), bits(0) {}
	BitStream(int reserved); // in uint64_t units
	BitStream(int size, uint64_t *buffer); //for reading


	~BitStream();
	void init(int size, uint64_t *buffer); //in uint64_t units for reading
	void reserve(int size); //for writing

	void write(uint64_t bits, int n);
	void read(int n, uint64_t &bits);

	void flush();
	int size; //in uint64
	uint64_t *buffer;
private:
	void push_back(uint64_t w);
	int allocated;

	uint64_t *pos;
	uint64_t buff;
	int bits;
};

class Obstream: public std::vector<uint64_t> {
public:
	Obstream();

	void write(uint64_t bits, int n); //write last n bits
	void flush();
private:
	uint64_t outbuff;
	int bitsToGo;
};


class Ibstream {
public:
	Ibstream(int size, uint64_t *buffer);

	void read(int n, uint64_t &bits);       // # of bits to read
	void rewind();                   // rewind to beginning of input
private:
	int size;
	uint64_t *buffer;                    // stream for reading bits
	uint64_t *pos; //keep track of current position

	uint64_t inbuff;                      // buffer bits for input
	int inbbits;                     // used for buffering
};

} //namespace
#endif // NX_BITSTREAM_H
