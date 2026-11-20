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
//#include <vcg/space/color4.h>
#include "renderer.h"
#include "nexus.h"
#include "token.h"
#include "controller.h"

#include <QDebug>
#include <chrono>
#include <thread>

#ifdef WIN32
//microsoft compiler does not provide it.
/*double log2( double n )
{
	// log(n)/log(2) is log2.
	return log( n )* 3.32192809;
}*/
#endif

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

void Renderer::verifyErrorSaturation(Nexus *instance) {
	const uint32_t n_nodes = instance->header.n_nodes;
	const uint32_t sink = n_nodes - 1;
	std::vector<float> screen_errors(n_nodes, 0.0f);

	for(uint32_t node = 0; node < sink; ++node) {
		bool visible;
		screen_errors[node] = metric.getError(instance->nodes[node].sphere,
		                                      instance->nodes[node].error, visible);
	}

	for(uint32_t parent = 0; parent < sink; ++parent) {
		const Node &node = instance->nodes[parent];
		for(uint32_t patch = node.first_patch; patch < node.last_patch(); ++patch) {
			const uint32_t child = instance->patches[patch].node;
			if(child == sink || screen_errors[child] <= screen_errors[parent])
				continue;

			qWarning() << "Screen-space error saturation violation: parent" << parent
			          << "(" << screen_errors[parent] << ") child" << child
			          << "(" << screen_errors[child] << ") stored errors"
			          << node.error << "->" << instance->nodes[child].error;
			return;
		}
	}
}

void Renderer::nearFar(Nexus *nexus, float &neard, float &fard) {
	
	if(!nexus->isReady()) return;
	
	Frustum &frustum = metric.frustum;
	nx::Sphere s = nexus->header.sphere;
	
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
		nx::Vector3f *points = data.coords();
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
	
	const auto time = std::chrono::steady_clock::now();
	if(stats.time != std::chrono::steady_clock::time_point{}) {
		int elapsed = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(time - stats.time).count());
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
			
			if(std::chrono::steady_clock::now() - time > std::chrono::milliseconds(wait))
				break;
			
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
	}
	errors.clear();
	errors.resize(nexus->header.n_nodes);

	//for debug purposes
	//verifyErrorSaturation(nexus);

	traverse(nexus);
	stats.instance_rendered = 0;
	
	const Signature &sig = nexus->header.signature;
	const RenderOptions options {
		sig.vertex.hasNormals() && (mode & NORMALS),
		sig.vertex.hasColors() && (mode & COLORS) && !(mode & PATCHES),
		sig.face.hasIndex() && (mode & TRIANGLES),
		nexus->header.n_textures && (mode & TEXTURES),
		sig.vertex.hasTextures() && (mode & TEXTURES)
	};

	beginRender(options);
	renderSelected(nexus, options);

	for(unsigned int i = 0; i < locked.size(); i++)
		locked[i]->unlock();
	locked.clear();
	
	stats.rendered += stats.instance_rendered;
	if(stats.instance_error > stats.error) stats.error = stats.instance_error;

	endRender(options);
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
			nx::Sphere sphere = node.tightSphere();
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

