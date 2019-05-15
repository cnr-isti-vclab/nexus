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
#define _FILE_OFFSET_BITS 64

#include "nexus.h"
#include "controller.h"
#include "globalgl.h"

#include <QGLWidget>


using namespace nx;

Nexus::Nexus(Controller *control): controller(control), loaded(false), http_stream(false) {
}

Nexus::~Nexus() {
	close();
}

bool Nexus::open(const char *_uri) {
	filename = _uri;

	url = std::string(_uri);

	if(url.compare(0, 7, "http://") == 0) {
		if(!controller)
			throw "No controller, no http streaming";
		http_stream = true;
	}

	if(url.compare(0, 7, "file://") == 0)
		url = url.substr(7, url.size());

	if(!isStreaming()) {
		file.setFileName(url.c_str());
		if(!file.open(QIODevice::ReadWrite))
			//file = fopen(_uri, "rb+");
			//if(!file)
			return false;
	}
	if(!controller) {
		try {
			loadHeader();
			loadIndex();
		} catch(const char *error) {
			return false;
		}
	} else {
		controller->load(this);
	}
	return true;
}

void Nexus::flush() {
	if(controller) {
		controller->flush(this);
		return;
	}
	NexusData::flush();
	delete []tokens;
}

void Nexus::initIndex() {
	NexusData::initIndex();
	tokens = new Token[header.n_nodes];
	for(uint i = 0; i < header.n_nodes; i++)
		tokens[i] = Token(this, i);
}

void Nexus::loadIndex(char *buffer) {
	NexusData::loadIndex(buffer);

	loaded = true;
}
void Nexus::loadIndex() {
	NexusData::loadIndex();

	loaded = true;
}


bool Nexus::isReady() {
	return loaded;
}

uint64_t Nexus::loadGpu(uint32_t n) {
	NodeData &data = nodedata[n];
	assert(data.memory);
	assert(data.vbo == 0);

	Node &node = nodes[n];

	Signature &sig = header.signature;
	uint32_t vertex_size = node.nvert*sig.vertex.size();
	uint32_t face_size = node.nface*sig.face.size();

	char *vertex_start = data.memory;
	char *face_start = vertex_start + vertex_size;

	assert(!glGetError());

	glGenBuffers(1, (GLuint *)(&(data.vbo)));
	glBindBuffer(GL_ARRAY_BUFFER, data.vbo);
	glBufferData(GL_ARRAY_BUFFER, vertex_size, vertex_start, GL_STATIC_DRAW);

	if(node.nface) {
		glGenBuffers(1, (GLuint *)(&(data.fbo)));
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, data.fbo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, face_size, face_start, GL_STATIC_DRAW);
	}

	int size = vertex_size + face_size;
	if(header.n_textures) {
		//be sure to load images
		for(uint32_t p = node.first_patch; p < node.last_patch(); p++) {
			Patch &patch = patches[p];
			uint32_t t = patch.texture;
			if(t == 0xffffffff) continue;

			TextureData &data = texturedata[t];
			data.count_gpu++;
			if(texturedata[t].tex) continue;

			assert(!glGetError());
			glGenTextures(1, &data.tex);
			glBindTexture(GL_TEXTURE_2D, data.tex);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, data.width, data.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data.memory);
			glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			
			//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

			
			assert(!glGetError());
			size += data.width*data.height*3;
			//careful with cache... might create problems to return different sizes in get drop and size
			/*			glGenerateMipmap(GL_TEXTURE_2D);  //Generate mipmaps now!!!
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR); */

		}
	}

	return size;
}

uint64_t Nexus::dropGpu(uint32_t n) {
	NodeData &data = nodedata[n];
	Node &node = nodes[n];

#ifndef NO_OPENGL
	glDeleteBuffers(1, (GLuint *)(&(data.vbo)));
	if(node.nface)
		glDeleteBuffers(1, (GLuint *)(&(data.fbo)));
#endif
	data.vbo = data.fbo = 0;

	Signature &sig = header.signature;
	uint32_t vertex_size = node.nvert*sig.vertex.size();
	uint32_t face_size = node.nface*sig.face.size();
	int size = vertex_size + face_size;

	if(header.n_textures) {
		//be sure to load images
		for(uint32_t p = node.first_patch; p < node.last_patch(); p++) {
			uint32_t t = patches[p].texture;
			if(t == 0xffffffff) continue;

			TextureData &tdata = texturedata[t];
			tdata.count_gpu--;
			if(tdata.count_gpu != 0) continue;

			glDeleteTextures(1, &tdata.tex);
			tdata.tex = 0;
			size += tdata.width*tdata.height*3;
		}
	}
	return size;
}

