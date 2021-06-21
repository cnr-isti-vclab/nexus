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
#ifndef NX_METRIC_H
#define NX_METRIC_H

#include "globalgl.h"
#include "frustum.h"
#include <vcg/space/sphere3.h>

#include <iostream>

namespace nx {

enum MetricKind { FRUSTUM = 0, FLAT = 1, DELTA = 2, ORTHO = 3 };

class Metric {
public:
	Frustum frustum;
	bool culling;

	Metric(): culling(true) {}
	void getView(const float *proj = NULL, const float *modelview = NULL, const int *viewport = NULL) {
		if(proj == NULL) {
#ifndef GL_ES
			float p[16];
			float m[16];
			int v[4];
			glGetFloatv(GL_PROJECTION_MATRIX, p);
			glGetFloatv(GL_MODELVIEW_MATRIX,m);
			glGetIntegerv(GL_VIEWPORT, v);
			frustum.setView(p, m, v);
#endif
		} else {
			frustum.setView(proj, modelview, viewport);
		}
	}

	float getError(vcg::Sphere3f &s, float error, bool &visible) {
		visible = true;
		float dist = (s.Center() - frustum.viewPoint()).Norm() - s.Radius();
		float zn = frustum.zNear();
		if(dist < zn) dist = zn;

		float res = frustum.resolution(dist);
		if(res <= 0.000001f) res = 0.000001f;
		error = error/res;
		if(culling) {
			float distance = frustum.distance(s.Center(), s.Radius());
			//if(distance < tight_radius - s.Radius())
			//    visible = false;

			if(distance < 0) {
				visible = false;
				float dr = s.Radius()/(-distance + s.Radius());
				if(dr > 1) exit(0);
				error *= dr*dr;
				error *= 0.001f;
			}
		}
		return error;
	}
};

}//namespace

#endif
