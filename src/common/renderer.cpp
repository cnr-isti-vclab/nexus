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
#include <vcg/space/color4.h>
#include <wrap/system/multithreading/util.h>
#include <wrap/system/time/clock.h>

#include "renderer.h"
#include "nexus.h"
#include "token.h"
#include "controller.h"

#include <QDebug>

#ifdef WIN32
//microsoft compiler does not provide it.
double log2( double n )
{
	// log(n)/log(2) is log2.
	return log( n )* 3.32192809;
}
#endif

extern int current_texture;

using namespace nx;


void Stats::resetAll() {
	//memset(this, 0, sizeof(Stats)); //fast hack.
	rendered = patch_rendered = node_rendered = instance_rendered = 0;
	frustum_culled = cone_culled = occlusion_culled = 0;
	error = instance_error = 0;
}

void Stats::resetCurrent() {
	instance_rendered = 0;
	instance_error = 0.0f;
}


void Renderer::startFrame() {
	stats.resetAll();
	frame++;
}


void Renderer::getView(const float *proj, const float *modelview, const int *viewport) {
	metric.getView(proj, modelview, viewport);
}

void Renderer::nearFar(Nexus *nexus, float &neard, float &fard) {
	
	if(!nexus->isReady()) return;
	
	Frustum &frustum = metric.frustum;
	vcg::Sphere3f s = nexus->header.sphere;
	
	float fd = 0;
	float nd = 1e20;
	frustum.updateNearFar(nd, fd, s.Center(), s.Radius());
	
	//if nd gets too small unnecessarily we lose precision in depth buffer.
	//needs to recourse to find closest but we cheat and just look at the closes vertex in the highest level
	if(nd <= frustum.scale()*s.Radius()/4.0f)
		nd = nexus->nodes[nexus->header.n_nodes-2].error * frustum.scale();
	
	/*        float nnd = 1e20;
		nx::Token *token = nexus->getToken(0);
		if(token->lock()) {
			Node &node = nexus->nodes[0];
			NodeData &data = nexus->data[0];
			vcg::Point3f *points = data.coords();
			for(uint i = 0; i < node.nvert; i++)
				frustum.updateNear(nnd, points[i]);
			token->unlock();
		}
		
		float min_distance = nexus->nodes[0].error * frustum.scale();
		
		if(nd == 1e20)
			nd = min_distance;
		else {
			//if we get closest than the approximation error of the first node we are very close :)
			float approximation = nexus->nodes[0].error * frustum.scale();
			nd -= approximation;
			if(nd < min_distance) nd = min_distance;
		}
	} */
	
	if(nd < neard) neard = nd;
	if(fd > fard) fard = fd;
}

void Renderer::render(Nexus *nexus, bool get_view, int wait ) {
	controller = nexus->controller;
	if(!nexus->isReady()) return;
	
	if(get_view) getView();
	
	
	locked.clear();
	last_node = 0;
	
	mt::Clock time = mt::Clock::currentTime();
	time.start();
	if(stats.time.isValid()) {
		int elapsed = stats.time.elapsed();
		if(elapsed > 0) {
			float fps = 1000.0f/elapsed;
			stats.fps = 0.1*fps + 0.9*stats.fps;
		}
	}
	
	stats.resetCurrent();
	stats.time = time;
	
	if(wait) {
		traverse(nexus);
		
		while(1) {
			if(nexus->controller->isWaiting())
				break;
			
			if(time.elapsed() > wait)
				break;
			
			mt::sleep_ms(10);
		}
	}
	errors.clear();
	errors.resize(nexus->header.n_nodes);
	traverse(nexus);
	stats.instance_rendered = 0;
	
	Signature &sig = nexus->header.signature;
	bool draw_normals = sig.vertex.hasNormals() && (mode & NORMALS);
	bool draw_colors = sig.vertex.hasColors() && (mode & COLORS ) && !(mode & PATCHES);
	bool draw_triangles = sig.face.hasIndex() && (mode & TRIANGLES);
	bool draw_textures = nexus->header.n_textures && (mode & TEXTURES);
	bool draw_texcoords = sig.vertex.hasTextures()&& (mode & TEXTURES);
	
	
	
	if(draw_textures)
		glEnable(GL_TEXTURE_2D);
	if(draw_textures && ! draw_texcoords) {
		glEnable(GL_TEXTURE_2D);
		glEnable(GL_TEXTURE_GEN_S);
		glEnable(GL_TEXTURE_GEN_T);
		glEnable(GL_TEXTURE_GEN_R);
		glEnable(GL_TEXTURE_GEN_Q);
	}
	
	glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
	glEnable(GL_COLOR_MATERIAL);
	
	glCheckError();
	
	
#ifdef GL_COMPATIBILITY
	glEnableClientState(GL_VERTEX_ARRAY);
	if(draw_colors) glEnableClientState(GL_COLOR_ARRAY);
	if(draw_normals) glEnableClientState(GL_NORMAL_ARRAY);
	if(draw_texcoords) glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	
	renderSelected(nexus);
	
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	if(draw_triangles) glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	
	glDisableClientState(GL_VERTEX_ARRAY);
	if(draw_normals) glDisableClientState(GL_NORMAL_ARRAY);
	if(draw_colors) glDisableClientState(GL_COLOR_ARRAY);
	if(draw_texcoords) glDisableClientState(GL_TEXTURE_COORD_ARRAY);
#endif
	
	glCheckError();
	
	
	
#if GL_ES || GL_CORE
	glEnableVertexAttribArray(ATTRIB_VERTEX);
	if(draw_colors) glEnableVertexAttribArray(ATTRIB_COLOR);
	if(draw_normals) glEnableVertexAttribArray(ATTRIB_NORMAL);
	
	renderSelected(nexus);
	
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	
	if(draw_triangles) glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	
	glDisableVertexAttribArray(ATTRIB_VERTEX);
	if(draw_normals) glDisableVertexAttribArray(ATTRIB_NORMAL);
	if(draw_colors) glDisableVertexAttribArray(ATTRIB_COLOR);
#endif
	
	for(unsigned int i = 0; i < locked.size(); i++)
		locked[i]->unlock();
	locked.clear();
	
	stats.rendered += stats.instance_rendered;
	if(stats.instance_error > stats.error) stats.error = stats.instance_error;
	
	
	if(draw_textures)
		glDisable(GL_TEXTURE_2D);
	if(draw_textures && ! draw_texcoords) {
		glDisable(GL_TEXTURE_GEN_S);
		glDisable(GL_TEXTURE_GEN_T);
		glDisable(GL_TEXTURE_GEN_R);
		glDisable(GL_TEXTURE_GEN_Q);
	}
	glDisable(GL_COLOR_MATERIAL);
}

void Renderer::endFrame() {
	if(controller)
		controller->endFrame();
	//current_error = stats.total_error;
	//current_rendered = stats.total_rendered;
	
	//porca trottola l'udpdate priorities.
	//non costa molto ma se ci sono un sacco di istance e' uno spreco.
	
	//tenere traccia dei controllers?
}

void Renderer::setMode(Renderer::Mode m, bool on) {
	if(on) mode |= m;
	else mode &= ~m;
}

void Renderer::renderSelected(Nexus *nexus) {
	
	glCheckError();
	
	Signature &sig = nexus->header.signature;
	bool draw_normals = sig.vertex.hasNormals() && (mode & NORMALS);
	bool draw_colors = sig.vertex.hasColors() && (mode & COLORS ) && !(mode & PATCHES);
	bool draw_triangles = sig.face.hasIndex() && (mode & TRIANGLES);
	bool draw_textures = nexus->header.n_textures && (mode & TEXTURES);
	bool draw_texcoords = sig.vertex.hasTextures()&& (mode & TEXTURES);
	
	/*
	//DEBUG ensure it is a valid cut:
	for(uint32_t i = 0; i < nexus->header.n_nodes-1; i++) {
		Node &node = nexus->nodes[i];
		//if a node is ! selected asser its children are not selected
		if(selected[i]) continue;
		for(uint32_t k = node.first_patch; k < node.last_patch(); k++) {
			Patch &patch = nexus->patches[k];
			assert(!selected[patch.node]);
		}
	}
*/
	int GPU_loaded = 0;
	uint32_t last_texture = 0xffffffff;
	for(uint32_t i = 0; i <= last_node; i++) {
		if(!selected[i]) continue;
		
		Node &node = nexus->nodes[i];
		
		if(nexus->header.signature.face.hasIndex() && skipNode(i)) continue;
		
		stats.node_rendered++;
		
		//TODO cleanup frustum..
		vcg::Sphere3f sphere = node.tightSphere();
		if(frustum_culling && metric.frustum.isOutside(sphere.Center(), sphere.Radius())) {
			stats.frustum_culled++;
			continue;
		}
		if(cone_culling && node.cone.Backface(sphere, metric.frustum.viewPoint())) {
			stats.cone_culled++;
			continue;
		}
		glCheckError();
		if(0) { //DEBUG
			vcg::Point3f c = sphere.Center();
			float r = node.tight_radius; //sphere.Radius(); //DEBUG
			
			glLineWidth(4);
			glBegin(GL_LINE_STRIP);
			for(int i = 0; i < 21; i++)
				glVertex3f(c[0] + r*sin(i*M_PI/10), c[1] + r*cos(i*M_PI/10), c[2]);
			glEnd();
		}
		
		glCheckError();
		NodeData &data = nexus->nodedata[i];
		assert(data.memory);
		
		if(!data.vbo) {
			GPU_loaded++;
			nexus->loadGpu(i);
		}
		
		assert(data.vbo);
		
		glBindBuffer(GL_ARRAY_BUFFER, data.vbo);
		uint64_t start = 0;

#ifdef GL_COMPATIBILITY
		glVertexPointer(3, GL_FLOAT, 0, (void *)start);
		start += node.nvert*sig.vertex.attributes[VertexElement::COORD].size();
		
		if(draw_texcoords)
			glTexCoordPointer(2, GL_FLOAT, 0, (void *)start);
		if(sig.vertex.hasTextures())
			start += node.nvert * sig.vertex.attributes[VertexElement::TEX].size();
		
		if(draw_normals)
			glNormalPointer(GL_SHORT, 0, (void *)start);
		if(sig.vertex.hasNormals())
			start += node.nvert*sig.vertex.attributes[VertexElement::NORM].size();
		
		if(draw_colors)
			glColorPointer(4, GL_UNSIGNED_BYTE, 0, (void *)start);
		if(sig.vertex.hasColors())
			start += node.nvert * sig.vertex.attributes[VertexElement::COLOR].size();
		
		
#endif
		
#if GL_ES || GL_CORE
		glVertexAttribPointer(ATTRIB_VERTEX, 3, GL_FLOAT, GL_TRUE, 0, (void *)start);
		
		start += node.nvert*sig.vertex.attributes[VertexElement::COORD].size();
		
		if(draw_normals)
			glVertexAttribPointer(ATTRIB_NORMAL, 3, GL_SHORT, GL_TRUE, 0, (void *)start);
		if(sig.vertex.hasNormals())
			start += node.nvert*sig.vertex.attributes[VertexElement::NORM].size();
		
		if(draw_colors)
			glVertexAttribPointer(ATTRIB_COLOR, 4, GL_UNSIGNED_BYTE, GL_TRUE, 0, (void *)start);
		if(sig.vertex.hasColors())
			start += node.nvert * sig.vertex.attributes[VertexElement::COLOR].size();
#endif
		
		if(mode & PATCHES) {
			vcg::Color4b  color;
			color[2] = ((i % 11)*171)%63 + 63;
			//color[1] = 255*log2((i+1))/log2(nexus->header.n_patches);
			color[1] = ((i % 7)*57)%127 + 127;
			color[0] = ((i % 16)*135)%127 + 127;
			//metric.getError(node.sphere, node.error, visible); //here we must use the saturated radius.
#ifdef GL_COMPATIBILITY
			glColor3ub(color[0], color[1], color[2]);
#endif
#if GL_ES || GL_CORE
			glVertexAttrib4f(ATTRIB_COLOR, color[0]/255.0f, color[1]/255.0f, color[2]/255.0f, 1.0f);
#endif
		}
		
		glCheckError();
		
		if(!draw_triangles) {
			//NexusController::instance().splat_renderer.Render(nvert, 6);
			//glPointSize(target_error);
			//float pointsize = 0.4*stats.instance_error;
			float pointsize = ceil(0.3*errors[i]); //2.0; //`0.1 * errors[i];
			//float pointsize = 1.2*stats.instance_error;
			if(pointsize > 2) pointsize = 2;
			glPointSize(pointsize);
			//glPointSize(4*target_error);
			
			glDrawArrays(GL_POINTS, 0, node.nvert);
			stats.patch_rendered++;
			stats.instance_rendered += node.nvert;
			continue;
		}
		
		if(draw_triangles)
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, data.fbo);
		
		uint32_t offset = 0;
		uint32_t end = 0;
		uint32_t last_patch = node.last_patch() - 1;
		for(uint32_t k = node.first_patch; k < node.last_patch(); k++) {
			Patch &patch = nexus->patches[k];
			
			if(!selected[patch.node]) { //we need to draw this
				end = patch.triangle_offset; //advance end
				if(k != last_patch)  //delay rendering
					continue;
			}
			if(end > offset) {
				
				//TODO GROUP TEXTURES ALSO
				if(draw_textures) {
					if(patch.texture != 0xffffffff) {
						if(last_texture != patch.texture) {
							//if(patch.texture == current_texture) {
							
							if(draw_textures && !draw_texcoords) {
								Texture &texture = nexus->textures[patch.texture];
								glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
								glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
								glTexGeni(GL_R, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
								glTexGeni(GL_Q, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
								
								glTexGenfv(GL_S, GL_OBJECT_PLANE, &texture.matrix[0]);
								glTexGenfv(GL_T, GL_OBJECT_PLANE, &texture.matrix[4]);
								glTexGenfv(GL_R, GL_OBJECT_PLANE, &texture.matrix[8]);
								glTexGenfv(GL_Q, GL_OBJECT_PLANE, &texture.matrix[12]);
							}
							TextureData &tdata = nexus->texturedata[patch.texture];
							glBindTexture(GL_TEXTURE_2D, tdata.tex);
							//glBindTexture(GL_TEXTURE_2D, 0);
							last_texture = patch.texture;
						}
					} else {
						glBindTexture(GL_TEXTURE_2D, 0);
						last_texture = 0xffffffff;
					}
				}
				
				if(!draw_triangles) {
					if(nexus->header.signature.face.hasIndex())
						glDrawArrays(GL_POINTS, 0, node.nvert);
					else
						glDrawArrays(GL_POINTS, offset, end - offset);
				} else {
					glDrawElements(GL_TRIANGLES, (end - offset)*3, GL_UNSIGNED_SHORT, (void *)(offset*3*sizeof(uint16_t)));
				}
				stats.patch_rendered++;
				stats.instance_rendered += end - offset;
			}
			offset = patch.triangle_offset;
		}
		glCheckError();
	}
	//	cout << "GPU loaded: " << GPU_loaded <<  endl;
}

Traversal::Action Renderer::expand(HeapNode h) {
	if(h.node > last_node) last_node = h.node;
	
	Nexus *nx = (Nexus *)nexus;
	nx::Token &token = nx->tokens[h.node];
	
	Priority newprio = Priority(h.error, frame);
	Priority oldprio = token.getPriority();
	if(oldprio < newprio)
		token.setPriority(newprio);
	
	nx->controller->addToken(&token);
	
	if(h.node != 0 && (h.error < target_error ||
					   (max_rendered && stats.instance_rendered > max_rendered))) {
		return BLOCK;
	}
	
	Node &node = nx->nodes[h.node];
	if(token.lock()) {
		stats.instance_error = h.error;
		errors[h.node] = h.error;
		if(h.visible) { //TODO check proper working on large models
			bool visible;
			vcg::Sphere3f sphere = node.tightSphere();
			metric.getError(sphere, node.error, visible);
			if(visible)
				stats.instance_rendered += node.nvert/2;
		}
		locked.push_back(&token);
		return EXPAND;
		
	} else {
		return BLOCK;
	}
}


float Renderer::nodeError(uint32_t n, bool &visible) {
	Node &node = ((Nexus *)nexus)->nodes[n];
	return metric.getError(node.sphere, node.error, visible); //here we must use the saturated radius.
	//vcg::Sphere3f sphere = node.tightSphere();
	//return metric.getError(sphere, node.error, visible); //here we must use the saturated radius.
}


