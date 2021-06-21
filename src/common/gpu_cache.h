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
#include <wrap/gcache/cache.h>

#include <QThread>
//#include <QWindow>
//#define SHARED_CONTEXT

#ifdef SHARED_CONTEXT
#include <QGLContext>
#endif

#include <iostream>

namespace nx {


class GpuCache: public vcg::Cache<nx::Token> {
public:
#ifndef SHARED_CONTEXT
	std::vector<unsigned int> to_drop;
	mt::mutex droplock;
#endif

#ifdef SHARED_CONTEXT
	QGLWidget *widget;
	QGLWidget *shared;

	GpuCache(): widget(NULL), shared(NULL) {}
#endif



#ifndef SHARED_CONTEXT
	void dropGpu() {
		mt::mutexlocker locker(&droplock);
		for(uint i = 0; i < to_drop.size(); i++)
			glDeleteBuffers(1, (GLuint *)&(to_drop[i]));
		to_drop.clear();
	}
#endif

#ifdef SHARED_CONTEXT
	void begin() {
		widget = new QGLWidget(NULL, shared);
		widget->show();
		widget->makeCurrent();
	}
	void end() {
		widget->doneCurrent();
		//        /context.doneCurrent();
	}
#endif

	int get(nx::Token *in) {
#ifdef SHARED_CONTEXT
		in->nexus->loadGpu(in->node);
		glFinish();
		mt::sleep_ms(1);
#endif
		//do nothing, transfer will be done in renderer
		return size(in);
	}
	int drop(nx::Token *in) {

#ifndef SHARED_CONTEXT
		//mark GPU to be dropped, it will be done in the renderer
		NodeData &data= in->nexus->nodedata[in->node];
		if(data.vbo == 0)  //can happen if a patch is scheduled for GPU but never actually rendererd
			return size(in);
		{
			mt::mutexlocker locker(&droplock);
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
