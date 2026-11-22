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
	enum Component { COORD = 0, NORM =1, COLOR = 2, TEX = 3, DATA0 = 4, DATA1 = 5 };
	static constexpr unsigned int COMPONENT_COUNT = 8;

	uint32_t component_offsets[COMPONENT_COUNT] = {};
	uint32_t vertex_size = 0;

	void setComponent(unsigned int c, Attribute attr) {
		Element::setComponent(c, attr);
		computeLayout();
	}

	void computeLayout() {
		uint32_t offset = 0;
		for(unsigned int c = 0; c < COMPONENT_COUNT; ++c) {
			if(attributes[c].isNull()) {
				component_offsets[c] = 0;
				continue;
			}
			offset = (offset + 3u) & ~3u;
			component_offsets[c] = offset;
			offset += attributes[c].size();
		}
		vertex_size = (offset + 3u) & ~3u;
	}

	uint32_t componentOffset(unsigned int c) const {
		assert(c < COMPONENT_COUNT);
		return component_offsets[c];
	}

	uint32_t vertexSize() const { return vertex_size; }

	void interleave(uint32_t count, const void *const *components, char *destination) const;
	void deinterleave(uint32_t count, const char *source, void *const *components) const;

	bool hasNormals() const { return !attributes[NORM].isNull(); }
	bool hasColors() const { return !attributes[COLOR].isNull(); }
	bool hasTextures() const { return !attributes[TEX].isNull(); }
	bool hasData(int i) const { return !attributes[DATA0 + i].isNull(); }
};

class FaceElement: public Element {
public:
	enum Component { INDEX = 0, NORM =1, COLOR = 2, TEX = 3, DATA0 = 4 };

	bool hasIndex() const { return !attributes[INDEX].isNull(); }
	bool hasNormals() const { return !attributes[NORM].isNull(); }
	bool hasColors() const { return !attributes[COLOR].isNull(); }
	bool hasTextures() const { return !attributes[TEX].isNull(); }
	bool hasData(int i) const { return !attributes[DATA0 + i].isNull(); }
};

class Signature {
public:
	VertexElement vertex;
	FaceElement face;

	enum Flags { PTEXTURE = 0x1, MECO = 0x2, CORTO = 0x4, DEEPZOOM = 0x8, INTERLEAVED = 0x10 };
	uint32_t flags;
	void setFlag(Flags f) { flags |= f; }
	void unsetFlag(Flags f) { flags &= ~f; }

	bool hasPTextures() { return (bool)(flags | PTEXTURE); }
	bool isCompressed() { return (bool)(flags & (MECO|CORTO)); }
	bool isDeepzoom() { return (bool)(flags & (DEEPZOOM)); }
	bool isInterleaved() const { return (flags & INTERLEAVED) != 0; }

	Signature(): flags(0) {}
};

}//namespace

#endif // NX_SIGNATURE_H
