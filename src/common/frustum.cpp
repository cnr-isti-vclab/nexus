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
#include <vcg/space/point4.h>
#include "frustum.h"

using namespace vcg;

void Frustum::setModel(const float *model) {
	Matrix44f imodel(model);
	imodel.transposeInPlace();
	imodel = Inverse(imodel);
	view_point = imodel * Point3f(0, 0, 0);
	view_dir = imodel * Point3f(0, 0, -1) - view_point;
	_scale = view_dir.SquaredNorm();
	view_dir /= _scale;
	_scale = 1.0f/sqrt(_scale);
}

void Frustum::setView(const float *_proj, const float *_model, const int *_viewport) {
	Matrix44f proj(_proj);
	proj.transposeInPlace();
	Matrix44f model(_model);
	model.transposeInPlace();
	int viewport[4];
	memcpy(viewport, _viewport, 4*sizeof(int));

	Matrix44f imodel = Inverse(model);
	view_point = imodel * Point3f(0, 0, 0);
	view_dir = imodel * Point3f(0, 0, -1) - view_point;
	_scale = view_dir.SquaredNorm();
	view_dir /= _scale;
	_scale = 1.0f/sqrt(_scale);

	//compute resolution
	Matrix44f iproj = Inverse(proj);
	Point3f c = iproj * Point3f(0, 0, -1); //center at distance znear
	Point3f s = iproj * Point3f(1, 0, -1); //side at distance znear;
	float zNear = c.Norm();
	float side = (s - c).Norm();
	_resolution = (2*side/zNear)/viewport[2];

	Matrix44f m = proj * model;
	m.transposeInPlace();
	float *v = m.V();

	//top
	planes[0] = Point4f(v[3] - v[1], v[7] - v[5], v[11] - v[9],  v[15] - v[13]);
	//right
	planes[1] = Point4f(v[3] - v[0], v[7] - v[4], v[11] - v[8],  v[15] - v[12]);
	//bottom
	planes[2] = Point4f(v[3] + v[1], v[7] + v[5], v[11] + v[9],  v[15] + v[13]);
	//left
	planes[3] = Point4f(v[3] + v[0], v[7] + v[4], v[11] + v[8],  v[15] + v[12]);
	//near
	planes[4] = Point4f(v[3] + v[2], v[7] + v[6], v[11] + v[10], v[15] + v[14]);
	//far
	planes[5] = Point4f(v[3] - v[2], v[7] - v[6], v[11] - v[10], v[15] - v[14]);

	//normalize to get correct distances
	for(int i = 0; i < 6; i++) {
		Point4f &p = planes[i];
		float d = sqrt(p[0]*p[0] + p[1]*p[1] + p[2]*p[2]);
		p /= d;
	}
}

vcg::Point3f Frustum::viewPoint() {
	return view_point;
}
//how many pixel a unit length segment projects on the screen at point p
//or the closest point of a sphere if radius > 0
float Frustum::resolution(vcg::Point3f p, float radius) {
	float dist = p * view_dir; //distance along the view axis
	dist -= radius * _scale;
	if(dist <= 0) return -1; //actually should return infinite...
	return _resolution * dist;
}

float Frustum::resolution(float dist) {
	return _resolution * dist;
}



bool Frustum::isOutside(vcg::Point3f center, float radius) {
	vcg::Point4f c(center[0], center[1], center[2], 1);

	for(int i = 0; i < 4; i++) {
		float d = planes[i] * c + radius;
		if(d < 0) return true;
	}
	return false;
}

float Frustum::distance(vcg::Point3f center, float radius) {
	float distance = 1e20f;
	vcg::Point4f c(center[0], center[1], center[2], 1);

	for(int i = 0; i < 4; i++) {
		float d = planes[i] * c + radius;
		if(d < distance) distance = d;
	}
	return distance; //usually negative
}


void Frustum::updateNearFar(float &znear, float &zfar, const vcg::Point3f &center, float radius) {
	float dist = (center - view_point) * view_dir; //distance along the view axis
	float dr = radius *_scale;
	if(znear > dist - dr) znear = dist - dr;
	if(zfar < dist + dr) zfar = dist + dr;
}

//(0, 0, -1, 1) * near_plane = zNear
//2zNear * zFar
//(zfar - znear)
void Frustum::updateNear(float &znear, const vcg::Point3f &p) {
	float dist = (p - view_point) * view_dir; //distance along the view axis
	if(znear > dist) znear = dist;
}
