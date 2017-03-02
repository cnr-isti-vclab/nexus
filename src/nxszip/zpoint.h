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
#ifndef NX_ZPOINT_H
#define NX_ZPOINT_H

#include <assert.h>
#include <stdint.h>
#include <vcg/space/point3.h>
#include <vcg/space/box3.h>

#include <iostream>
using namespace std;

class ZPoint {
public:
	uint64_t bits; //enough for 21 bit quantization.

public:
	int pos;//used after sorting to recover reordering
	ZPoint(uint64_t b = 0): bits(b) {}
	ZPoint(uint64_t x, uint64_t y, uint64_t z, int levels, int _pos):bits(0), pos(_pos) {
		uint64_t l = 1;
		for (int i = 0; i < levels; i++) {
			bits |= (x & l << i)<< (2*i) | (y & l << i) << (2*i + 1) | (z & l << i) << (2*i+2);
		}
	}
	ZPoint(vcg::Point3f p, vcg::Box3f &box, int levels, int _pos):
		bits(0), pos(_pos) {
		p = box.GlobalToLocal(p);
		p *= (1<<levels);

		uint64_t x = (uint64_t)p[0];
		uint64_t y = (uint64_t)p[1];
		uint64_t z = (uint64_t)p[2];
		uint64_t l = 1;
		for (int i = 0; i < levels; i++) {
			bits |= (x & l << i)<< (2*i) | (y & l << i) << (2*i + 1) | (z & l << i) << (2*i+2);
		}
		/*    spread(x);
	spread(y);
	spread(z);
	bits = x | (y<<1) | (z<<2);
//    assert( bits == x | (y<<1) | (z<<2)); */


	}
	/*  void spread(uint64_t &x) {
	// 0000_0000_0001_1111_1111_1111_1111_1111
	x = ( x | ( x << 32 ) ) & 0xffff00000000ffff;
	// 0000_0000_0000_0000_1111_1111_1111_1111
	x = ( x | ( x << 16 ) ) & 0x00ff0000ff0000ff;
	// 1111_1111_0000_0000_0000_0000_1111_1111
	x = ( x | ( x << 8 ) )  & 0xf00f00f00f00f00f;
	// 0000_0011_0000_0000_1111_0000_0000_1111
	x = ( x | ( x << 4 ) )  & 0x30c30c30c30c30c3;
	// 0000_0011_0000_1100_0011_0000_1100_0011
	x = ( x | ( x << 2 ) )  & 0x9249249249249249;
	// 0000_1001_0010_0100_1001_0010_0100_1001


  }*/

	uint64_t morton2(uint64_t x)
	{
		x = x & 0x55555555ull;
		x = (x | (x >> 1)) & 0x33333333ull;
		x = (x | (x >> 2)) & 0x0F0F0F0Full;
		x = (x | (x >> 4)) & 0x00FF00FFull;
		x = (x | (x >> 8)) & 0x0000FFFFull;
		return x;
	}

	uint64_t morton3(uint64_t x)
	{
		//1001 0010  0100 1001  0010 0100  1001 0010  0100 1001  0010 0100  1001 0010  0100 1001 => 249
		//1001001001001001001001001001001001001001001001001001001001001001
		//a  b  c  d  e  f  g  h  i  l  m  n  o  p  q  r  s  t  u  v  x  y
		// a  b  c  d  e  f  g  h  i  l  m  n  o  p  q  r  s  t  u  v  x
		//  a  b  c  d  e  f  g  h  i  l  m  n  o  p  q  r  s  t  u  v  x
		//
		//0011000011000011000011000011000011000011000011000011000011000011
		x = x & 0x9249249249249249ull;
		x = (x | (x >> 2))  & 0x30c30c30c30c30c3ull;
		x = (x | (x >> 4))  & 0xf00f00f00f00f00full;
		x = (x | (x >> 8))  & 0x00ff0000ff0000ffull;
		x = (x | (x >> 16)) & 0xffff00000000ffffull;
		x = (x | (x >> 32)) & 0x00000000ffffffffull;
		return x;
	}

	vcg::Point3f toPoint(const vcg::Point3i &min, float step) {
		uint32_t x = morton3(bits);
		uint32_t y = morton3(bits>>1);
		uint32_t z = morton3(bits>>2);

		/*uint32_t x = 0;
		uint32_t y = 0;
		uint32_t z = 0;
		uint64_t l = 1;
		for(int i = 0; i < 22; i++) {
			x |= (bits & (l<< (3*i))) >> (2*i);
			y |= (bits & (l<< (3*i + 1))) >> (2*i+1);
			z |= (bits & (l<< (3*i + 2))) >> (2*i+2);
		} */

		vcg::Point3f p((int)x + min[0], (int)y + min[1], (int)z + min[2]);
		p *= step;
		return p;
	}

	vcg::Point3f toPoint(vcg::Box3f &box, int levels) {
		uint32_t x = 0;
		uint32_t y = 0;
		uint32_t z = 0;
		uint64_t l = 1;
		for(int i = 0; i < levels; i++) {
			x |= (bits & (l<< (3*i))) >> (2*i);
			y |= (bits & (l<< (3*i + 1))) >> (2*i+1);
			z |= (bits & (l<< (3*i + 2))) >> (2*i+2);
		}
		vcg::Point3f p(x, y, z);
		p /= (1<<levels);
		p = box.LocalToGlobal(p);
		return p;
	}
#ifdef WIN32
#define ONE64 ((__int64)1)
#else
#define ONE64 1LLU
#endif
	void clearBit(int i) {
		bits &= ~(ONE64 << i);
	}
	void setBit(int i) {
		bits |= (ONE64 << i);
	}

	void setBit(int i, uint64_t val) {
		bits &= ~(ONE64 << i);
		bits |= (val << i);
	}
	bool testBit(int i) {
		return (bits & (ONE64<<i)) != 0;
	}
	bool operator!=(const ZPoint &zp) const {
		return bits != zp.bits;
	}
	bool operator<(const ZPoint &zp) const {
		return bits > zp.bits;
	}
	int difference(const ZPoint &p) const {
		uint64_t diff = bits ^ p.bits;
		return log2(diff);
	}
	static int log2(uint64_t p) {
		int k = 0;
		while ( p>>=1 ) { ++k; }
		return k;
	}
	void debug() {
		for(int i = 0; i < 64; i++)
			if(testBit(i)) cout << '1';
			else cout << '0';
	}
};

#endif // NX_ZPOINT_H
