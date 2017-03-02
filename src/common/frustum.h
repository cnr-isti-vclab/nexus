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

#include <vcg/math/matrix44.h>
#include <vcg/space/plane3.h>

class Frustum {
public:

	void setView(const float *_proj, const float *_model, const int *_viewport);

	//allows only computation of updateNear and ofviewpoint and viewdir
	void setModel(const float *_model);

	//view point in model space
	vcg::Point3f viewPoint();

	//how many pixel a unit length segment projects on the screen at point p
	//or the closest point of a sphere if radius > 0
	float resolution(vcg::Point3f p, float radius = 0);

	//return scale between model space and view space
	float scale() { return _scale; }

	//same as before.
	float resolution(float dist = 1);

	bool isOutside(vcg::Point3f center, float radius = 0);
	float distance(vcg::Point3f center, float radius = 0);

	void updateNearFar(float &znear, float &zfar, const vcg::Point3f &center, float radius = 0);
	void updateNear(float &znear, const vcg::Point3f &p);

	float zNear() { return -planes[4]*vcg::Point4f(view_point[0], view_point[1], view_point[2], 1); }
	float zFar() { return planes[5]*vcg::Point4f(view_point[0], view_point[1], view_point[2], 1); }

protected:
	float _resolution;
	float _scale; //to scale a radius from model space to view space
	//top right bottom left, near, far, the normal points inward
	vcg::Point4f planes[6];
	vcg::Point3f view_point;
	vcg::Point3f view_dir;
};

#endif // NX_FRUSTUM_H
