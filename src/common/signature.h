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
#ifndef NX_SIGNATURE_H
#define NX_SIGNATURE_H

#ifdef _MSC_VER
typedef __int16 int16_t;
typedef unsigned __int16 uint16_t;
typedef __int32 int32_t;
typedef unsigned __int32 uint32_t;
typedef __int64 int64_t;
typedef unsigned __int64 uint64_t;
#else
#include <stdint.h>
#endif

#include <assert.h>

namespace nx {

class Attribute {
public:
	enum Type { NONE = 0, BYTE, UNSIGNED_BYTE, SHORT, UNSIGNED_SHORT, INT, UNSIGNED_INT, FLOAT, DOUBLE };
	unsigned char type;
	unsigned char number;

	Attribute(): type(0), number(0) {}

	Attribute(Type t, unsigned char n): type(t), number(n) {}
	int size() const {
		static unsigned char typesize[] = { 0, 1, 1, 2, 2, 4, 4, 4, 8 };
		return number*typesize[type];
	}
	bool isNull() const { return type == 0; }
};

class Element {
public:
	Attribute attributes[8];
	void setComponent(unsigned int c, Attribute attr) {
		assert(c < 8);
		attributes[c] = attr;
	}
	int size() const {
		int s = 0;
		for(unsigned int i = 0; i < 8; i++)
			s += attributes[i].size();
		return s;
	}
};

class VertexElement: public Element {
public:
	enum Component { COORD = 0, NORM =1, COLOR = 2, TEX = 3, DATA0 = 4 };

	bool hasNormals() { return !attributes[NORM].isNull(); }
	bool hasColors() { return !attributes[COLOR].isNull(); }
	bool hasTextures() const { return !attributes[TEX].isNull(); }
	bool hasData(int i) { return !attributes[DATA0 + i].isNull(); }
};

class FaceElement: public Element {
public:
	enum Component { INDEX = 0, NORM =1, COLOR = 2, TEX = 3, DATA0 = 4 };

	bool hasIndex() { return !attributes[INDEX].isNull(); }
	bool hasNormals() { return !attributes[NORM].isNull(); }
	bool hasColors() { return !attributes[COLOR].isNull(); }
	bool hasTextures() { return !attributes[TEX].isNull(); }
	bool hasData(int i) { return !attributes[DATA0 + i].isNull(); }
};

class Signature {
public:
	VertexElement vertex;
	FaceElement face;

	enum Flags { PTEXTURE = 0x1, MECO = 0x2, CORTO = 0x4 };
	uint32_t flags;
	void setFlag(Flags f) { flags |= f; }
	void unsetFlag(Flags f) { flags &= ~f; }

	bool hasPTextures() { return (bool)(flags | PTEXTURE); }
	bool isCompressed() { return (bool)(flags & (MECO|CORTO)); }

	Signature(): flags(0) {}
};

}//namespace

#endif // NX_SIGNATURE_H
