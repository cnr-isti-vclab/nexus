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
#include "controller.h"

using namespace nx;

//TODO how to auto-guess ram and gpu?
Controller::Controller(): max_tokens(300), max_ram(512*1000*1000), max_gpu(256*1000*1000) {
	ram_cache.setCapacity(max_ram);
	gpu_cache.setCapacity(max_gpu);

	addCache(&ram_cache);
	addCache(&gpu_cache);
	//setMaxTokens(max_tokens);
}

void Controller::setWidget(QGLWidget *widget) {
	//TODO check destruction of widget
#ifdef SHARED_CONTEXT
	gpu_cache.shared = widget; //new QGLWidget(NULL, widget);
#endif
}

void Controller::load(Nexus *nexus) {
	ram_cache.add(nexus);
	ram_cache.input->check_queue.open();
}

void Controller::flush(Nexus *nexus) {
	TokenRemover remover(nexus);
	removeTokens(remover);
}

bool Controller::hasCacheChanged() {
	return newData();
}

void Controller::endFrame() {
#ifndef SHARED_CONTEXT
	gpu_cache.dropGpu();
#endif
	updatePriorities();
}
