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
#ifndef NX_CONTROLLER_H
#define NX_CONTROLLER_H

#include <wrap/gcache/controller.h>

#include "token.h"
#include "ram_cache.h"
#include "gpu_cache.h"


class QGLWidget;

namespace nx {

class Nexus;

class TokenRemover {
public:
	Nexus *nexus;
	TokenRemover(Nexus *n): nexus(n) {}
	bool operator()(vcg::Token<Priority> *t) {
		nx::Token *n = (nx::Token *)t;
		return n->nexus == nexus;
	}
};

class Controller: public vcg::Controller<Token> {
public:
	RamCache ram_cache;
	GpuCache gpu_cache;

	Controller();
	~Controller() { finish(); }


	void setWidget(QGLWidget *widget);

	void setGpu(uint64_t size) { max_gpu = size; gpu_cache.setCapacity(size); }
	uint64_t maxGpu() const { return max_gpu; }

	void setRam(uint64_t size) { max_ram = size; ram_cache.setCapacity(size); }
	uint64_t maxRam() const { return max_ram; }

	void load(Nexus *nexus);
	void flush(Nexus *nexus);
	bool hasCacheChanged();
	void endFrame();

	void dropGpu();
protected:
	uint64_t max_tokens, max_ram, max_gpu;    //sizes for cache and (max gpu actually limits max tri per frame?)
};


}

#endif // NX_CONTROLLER_H
