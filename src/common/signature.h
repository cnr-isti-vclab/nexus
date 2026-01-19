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

#include "config.h"
#include <vector>
#include <assert.h>
namespace nx {

class Attribute {
public:
	enum Type { NONE = 0, BYTE, UNSIGNED_BYTE, SHORT, UNSIGNED_SHORT, INT, UNSIGNED_INT, FLOAT, DOUBLE };
	unsigned char type   = 0;
	unsigned char number = 0;
	unsigned char offset = 0;

	Attribute() {}
	Attribute(Type t, unsigned char n): type(t), number(n), offset(0) {}
	int size() {
		static unsigned char typesize[] = { 0, 1, 1, 2, 2, 4, 4, 4, 8 };
		return number*typesize[type];
	}
	bool isNull() { return type == 0; }
};

class Element {
public:
	Attribute attributes[8];
	void setComponent(unsigned int c, Attribute attr) {
		assert(c < 8);
		attributes[c] = attr;
	}
	int size() {
		int s = 0;
		for(unsigned int i = 0; i < 8; i++)
			s += attributes[i].size();
		return s;
	}
	//stride with 4bytes alignment.
	int stride() {
		int s = 0;
		for(unsigned int i = 0; i < 8; i++) {
			int k = attributes[i].size() + 3;
			k -= k & 0x2;
			attributes[i].offset = s; //TODO ugly hack to recompute offsets./
			s += k;
		}
		return s;
	}
};

class VertexElement: public Element {
public:
	//TODO remane all components to gltf names
	enum Component { POSITION = 0, NORM =1, COLOR = 2, UV = 3, UV_1 = 4, TANGENT= 5, WEIGHTS= 6, JOINTS= 7 };
	bool has(Component c) { return !attributes[c].isNull(); }
	
	bool hasNormals() { return !attributes[NORM].isNull(); }
	bool hasColors() { return !attributes[COLOR].isNull(); }
	bool hasTextures() { return !attributes[UV].isNull(); }
};

class FaceElement: public Element {
public:
	enum Component { INDEX = 0, NORM =1, COLOR = 2, TEX = 3, DATA0 = 4 };

	bool has(Component c) { return !attributes[c].isNull(); }		
	bool hasIndex() { return !attributes[INDEX].isNull(); }

};


class Signature {
public:
	VertexElement vertex;
	FaceElement face;
	enum Flags { PTEXTURE = 0x1, MECO = 0x2, CORTO = 0x4, DEEPZOOM = 0x8 };
	uint32_t flags = 0;
	void setFlag(Flags f) { flags |= f; }
	void unsetFlag(Flags f) { flags &= ~f; }

	bool hasPTextures() { return (bool)(flags | PTEXTURE); }
	bool isCompressed() { return (bool)(flags & (MECO|CORTO)); }
	bool isDeepzoom() { return (bool)(flags & (DEEPZOOM)); }

	Signature(): flags(0) {}
};

}//namespace

#endif // NX_SIGNATURE_H
