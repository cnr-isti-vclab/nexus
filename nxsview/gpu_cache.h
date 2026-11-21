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
#ifndef NX_GPU_CACHE_H
#define NX_GPU_CACHE_H

#include "globalgl.h"
#include "nexus.h"
#include "token.h"
#include "../src/gcache/cache.h"

#include <chrono>
#include <thread>
//#include <QWindow>
//#define SHARED_CONTEXT

#if defined(SHARED_CONTEXT)
#include <QOpenGLWidget>
#endif

#include <iostream>
#include <mutex>

namespace nx {


class GpuCache: public vcg::Cache<nx::Token> {
public:
	bool openGLBackend = true;

	void setOpenGLBackend(bool enabled) { openGLBackend = enabled; }

#if !defined(SHARED_CONTEXT)
	std::vector<unsigned int> to_drop;
	std::mutex droplock;
#endif

#if defined(SHARED_CONTEXT)
	QOpenGLWidget *widget;
	QOpenGLWidget *shared;

	GpuCache(): widget(NULL), shared(NULL) {}
#endif



#if !defined(SHARED_CONTEXT)
	void dropGpu() {
		std::lock_guard<std::mutex> locker(droplock);
		if(openGLBackend) {
			for(uint i = 0; i < to_drop.size(); i++)
				glDeleteBuffers(1, (GLuint *)&(to_drop[i]));
		}
		to_drop.clear();
	}
#endif

#if defined(SHARED_CONTEXT)
	void begin() {
		Q_UNUSED(shared);
		widget = new QOpenGLWidget();
		widget->show();
		widget->makeCurrent();
	}
	void end() {
		widget->doneCurrent();
		//        /context.doneCurrent();
	}
#endif

	int get(nx::Token *in) {
#if defined(SHARED_CONTEXT)
		if(openGLBackend) {
			in->nexus->loadGpu(in->node);
			glFinish();
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
#endif
		//do nothing, transfer will be done in renderer
		return size(in);
	}
	int drop(nx::Token *in) {

#if !defined(SHARED_CONTEXT)
		//mark GPU to be dropped, it will be done in the renderer
		NodeData &data= in->nexus->nodedata[in->node];
		if(!openGLBackend) {
			data.vbo = data.fbo = 0;
			return size(in);
		}
		if(data.vbo == 0)  //can happen if a patch is scheduled for GPU but never actually rendererd
			return size(in);
		{
			std::lock_guard<std::mutex> locker(droplock);
			if(data.vbo) {
				to_drop.push_back(data.vbo);
				data.vbo = 0;
			}
			if(data.fbo) {
				to_drop.push_back(data.fbo);
				data.fbo = 0;
			}
		}
#else
		if(openGLBackend)
			in->nexus->dropGpu(in->node);
#endif

		return size(in);
	}

	int size(nx::Token *in) {
		Node &node = in->nexus->nodes[in->node];
		Signature &sig = in->nexus->header.signature;

		uint32_t vertex_size = node.nvert*sig.vertex.size();
		uint32_t face_size = node.nface*sig.face.size();
		int size = vertex_size + face_size;
		if(in->nexus->header.n_textures) {
			for(uint32_t p = node.first_patch; p < node.last_patch(); p++) {
				uint32_t t = in->nexus->patches[p].texture;
				if(t == 0xffffffff) continue;

				TextureData &tdata = in->nexus->texturedata[t];
				std::lock_guard<std::mutex> texture_locker(tdata.mutex);
				size += tdata.width*tdata.height*3;
				break;
			}
		}
		return size;
	}

	int size() { return Cache<nx::Token>::size(); }
};

} //namespace

#endif // NX_GPU_CACHE_H
