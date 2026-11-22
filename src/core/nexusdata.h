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
#ifndef NX_NEXUSDATA_H
#define NX_NEXUSDATA_H

#include <stdio.h>
#include <array>
#include <cstddef>
#include <cstring>
#include <limits>
#include <string>
#include <mutex>
#include <stdexcept>
#include <vector>

#include "signature.h"
#include "dag.h"
#include "mesh_types.h"

#include "nexusfile.h"

namespace nx {

class PlanarVertexData {
public:
	PlanarVertexData(const VertexElement &layout, uint32_t vertexCount):
		layout(layout), vertexCount(vertexCount) {
		for(unsigned int c = 0; c < VertexElement::COMPONENT_COUNT; ++c) {
			const std::size_t bytes = layout.attributes[c].size();
			if(bytes && vertexCount > std::numeric_limits<std::size_t>::max() / bytes)
				throw std::length_error("planar vertex buffer size overflows");
			components[c].resize(static_cast<std::size_t>(vertexCount) * bytes);
		}
	}

	char *component(unsigned int c) {
		assert(c < VertexElement::COMPONENT_COUNT);
		return components[c].empty() ? nullptr : components[c].data();
	}

	const char *component(unsigned int c) const {
		assert(c < VertexElement::COMPONENT_COUNT);
		return components[c].empty() ? nullptr : components[c].data();
	}

	void deinterleave(const char *interleaved) {
		std::array<void *, VertexElement::COMPONENT_COUNT> destinations{};
		for(unsigned int c = 0; c < VertexElement::COMPONENT_COUNT; ++c)
			destinations[c] = component(c);
		layout.deinterleave(vertexCount, interleaved, destinations.data());
	}

	void readPlanar(const char *source) {
		std::size_t offset = 0;
		for(unsigned int c : planarComponentOrder()) {
			const std::size_t bytes = static_cast<std::size_t>(layout.attributes[c].size()) * vertexCount;
			if(!bytes)
				continue;
			if(!source)
				throw std::invalid_argument("missing planar vertex data");
			std::memcpy(component(c), source + offset, bytes);
			offset += bytes;
		}
	}

	void writePlanar(char *destination) const {
		std::size_t offset = 0;
		for(unsigned int c : planarComponentOrder()) {
			const std::size_t bytes = static_cast<std::size_t>(layout.attributes[c].size()) * vertexCount;
			if(!bytes)
				continue;
			if(!destination)
				throw std::invalid_argument("missing planar vertex destination");
			std::memcpy(destination + offset, component(c), bytes);
			offset += bytes;
		}
	}

	std::size_t planarVertexSize() const {
		return layout.size();
	}

	void interleave(char *interleaved) const {
		std::array<const void *, VertexElement::COMPONENT_COUNT> sources{};
		for(unsigned int c = 0; c < VertexElement::COMPONENT_COUNT; ++c)
			sources[c] = component(c);
		layout.interleave(vertexCount, sources.data(), interleaved);
	}

private:
	static const std::array<unsigned int, VertexElement::COMPONENT_COUNT> &planarComponentOrder() {
		static const std::array<unsigned int, VertexElement::COMPONENT_COUNT> order = {
			VertexElement::COORD, VertexElement::TEX, VertexElement::NORM, VertexElement::COLOR,
			VertexElement::DATA0, VertexElement::DATA0 + 1,
			VertexElement::DATA0 + 2, VertexElement::DATA0 + 3
		};
		return order;
	}

	const VertexElement &layout;
	uint32_t vertexCount;
	std::array<std::vector<char>, VertexElement::COMPONENT_COUNT> components;
};

class NodeData {
public:
	NodeData(): memory(NULL), vbo(0), fbo(0) {}
	char *memory;
	uint32_t vbo;
	uint32_t fbo;

	uint16_t *faces(Signature &sig, uint32_t nvert) {
		return faces(sig, nvert, memory);
	}

	static uint16_t *faces(Signature &sig, uint32_t nvert, char *mem);
};

class TextureData {
public:
	std::mutex mutex;
	TextureData(): memory(NULL), width(0), height(0), tex(0), count_ram(0), count_gpu(0) {}
	char *memory;             //some form of raw data (probably compressed jpeg)
	uint32_t width;            //size of image
	uint32_t height;

	uint32_t tex;             //opengl identifier
	uint32_t count_ram;           //number of nodes using the texture
	uint32_t count_gpu;           //number of nodes using the texture
};


class NexusData {
public:
	Header header;
	Node *nodes;
	Patch *patches;
	Texture *textures;

	NodeData *nodedata;
	TextureData *texturedata;

	std::string url;
	uint32_t nroots;

	NexusData();
	virtual ~NexusData();
	bool open(const char *uri);
	void close();
	virtual void flush();

	Sphere &boundingSphere() { return header.sphere; }
	bool intersects(Ray3 &r, float &distance);

	uint32_t size(uint32_t node);
	uint64_t loadRam(uint32_t node);
	uint64_t dropRam(uint32_t node, bool write = false);

	void loadHeader();
	void loadHeader(char *buffer);
	uint64_t indexSize();
	void countRoots();
	virtual void initIndex();
	virtual void loadIndex();
	virtual void loadIndex(char *buffer);

	virtual void loadImageFromData(TextureData& data, int textureIndex) { };

	//FILE *file;

	NexusFile* file;
	///Serializes access to the stateful NexusFile object across RAM workers.
	std::mutex file_mutex;
};

} //namespace

#endif // NX_NEXUSDATA_H
