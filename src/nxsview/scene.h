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
#ifndef NX_SCENE_H
#define NX_SCENE_H

#include "../common/controller.h"
#include <QStringList>
#include <vcg/math/matrix44.h>

namespace nx {
class Nexus;
class Controller;
}

class Scene {
public:
	class Node {
	public:
		nx::Nexus *nexus;
		vcg::Matrix44f transform;
		bool inited;

		Node(nx::Nexus *in): nexus(in), inited(false) {
			transform.SetIdentity();
		}
	};
	std::vector<nx::Nexus *> models;
	std::vector<Node> nodes;
	nx::Controller controller;
	bool autopositioning;

	Scene(): autopositioning(false) {}
	~Scene();
	bool load(QStringList input, int instances);
	void update();
};

#endif // NX_SCENE_H
