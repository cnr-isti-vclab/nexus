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
#ifndef NX_FRUSTUM_H
#define NX_FRUSTUM_H

#include "../core/mesh_types.h"

class Frustum {
public:

	void setView(const float *_proj, const float *_model, const int *_viewport);

	//allows only computation of updateNear and ofviewpoint and viewdir
	void setModel(const float *_model);

	//view point in model space
	nx::Vector3f viewPoint();

	//how many pixel a unit length segment projects on the screen at point p
	//or the closest point of a sphere if radius > 0
	float resolution(const nx::Vector3f &p, float radius = 0);

	//return scale between model space and view space
	float scale() { return _scale; }

	//same as before.
	float resolution(float dist = 1);

	bool isOutside(const nx::Vector3f &center, float radius = 0);
	float distance(const nx::Vector3f &center, float radius = 0);

	void updateNearFar(float &znear, float &zfar, const nx::Vector3f &center, float radius = 0);
	void updateNear(float &znear, const nx::Vector3f &p);

	float zNear() { return -planeDistance(planes[4], view_point); }
	float zFar() { return planeDistance(planes[5], view_point); }

protected:
	float _resolution;
	float _scale; //to scale a radius from model space to view space
	//top right bottom left, near, far, the normal points inward
	float planes[6][4]{};
	nx::Vector3f view_point{};
	nx::Vector3f view_dir{};
	static float planeDistance(const float *plane, const nx::Vector3f &point) {
		return plane[0]*point.x + plane[1]*point.y + plane[2]*point.z + plane[3];
	}
};

#endif // NX_FRUSTUM_H
