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
#include "frustum.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace {

nx::Matrix4f inverse(const nx::Matrix4f &matrix) {
	double a[4][8]{};
	for(int row = 0; row < 4; ++row) {
		for(int col = 0; col < 4; ++col)
			a[row][col] = matrix.m[col*4 + row];
		a[row][row + 4] = 1.0;
	}
	for(int col = 0; col < 4; ++col) {
		int pivot = col;
		for(int row = col + 1; row < 4; ++row)
			if(std::abs(a[row][col]) > std::abs(a[pivot][col])) pivot = row;
		if(std::abs(a[pivot][col]) < 1e-15) return {};
		if(pivot != col)
			for(int k = 0; k < 8; ++k) std::swap(a[pivot][k], a[col][k]);
		double divisor = a[col][col];
		for(int k = 0; k < 8; ++k) a[col][k] /= divisor;
		for(int row = 0; row < 4; ++row) {
			if(row == col) continue;
			double factor = a[row][col];
			for(int k = 0; k < 8; ++k) a[row][k] -= factor * a[col][k];
		}
	}
	nx::Matrix4f result{};
	for(int row = 0; row < 4; ++row)
		for(int col = 0; col < 4; ++col)
			result.m[col*4 + row] = static_cast<float>(a[row][col + 4]);
	return result;
}

nx::Vector3f transformPoint(const nx::Matrix4f &matrix, const nx::Vector3f &point) {
	float x = matrix.m[0]*point.x + matrix.m[4]*point.y + matrix.m[8]*point.z + matrix.m[12];
	float y = matrix.m[1]*point.x + matrix.m[5]*point.y + matrix.m[9]*point.z + matrix.m[13];
	float z = matrix.m[2]*point.x + matrix.m[6]*point.y + matrix.m[10]*point.z + matrix.m[14];
	float w = matrix.m[3]*point.x + matrix.m[7]*point.y + matrix.m[11]*point.z + matrix.m[15];
	if(w != 0.0f) return {x/w, y/w, z/w};
	return {x, y, z};
}

}

void Frustum::setModel(const float *model) {
	nx::Matrix4f model_matrix{};
	std::memcpy(model_matrix.m, model, sizeof(model_matrix.m));
	nx::Matrix4f imodel = inverse(model_matrix);
	view_point = transformPoint(imodel, {0, 0, 0});
	view_dir = transformPoint(imodel, {0, 0, -1}) - view_point;
	_scale = view_dir.squaredNorm();
	view_dir /= _scale;
	_scale = 1.0f/sqrt(_scale);
}

void Frustum::setView(const float *_proj, const float *_model, const int *_viewport) {
	nx::Matrix4f proj{};
	nx::Matrix4f model{};
	std::memcpy(proj.m, _proj, sizeof(proj.m));
	std::memcpy(model.m, _model, sizeof(model.m));
	int viewport[4];
	std::memcpy(viewport, _viewport, 4*sizeof(int));

	nx::Matrix4f imodel = inverse(model);
	view_point = transformPoint(imodel, {0, 0, 0});
	view_dir = transformPoint(imodel, {0, 0, -1}) - view_point;
	_scale = view_dir.squaredNorm();
	view_dir /= _scale;
	_scale = 1.0f/sqrt(_scale);

	//compute resolution
	nx::Matrix4f iproj = inverse(proj);
	nx::Vector3f c = transformPoint(iproj, {0, 0, -1}); //center at distance znear
	nx::Vector3f s = transformPoint(iproj, {1, 0, -1}); //side at distance znear;
	float zNear = c.norm();
	float side = (s - c).norm();
	_resolution = (2*side/zNear)/viewport[2];

	nx::Matrix4f m = proj * model;
	float *v = m.m;

	//top
	planes[0][0] = v[3] - v[1]; planes[0][1] = v[7] - v[5]; planes[0][2] = v[11] - v[9]; planes[0][3] = v[15] - v[13];
	//right
	planes[1][0] = v[3] - v[0]; planes[1][1] = v[7] - v[4]; planes[1][2] = v[11] - v[8]; planes[1][3] = v[15] - v[12];
	//bottom
	planes[2][0] = v[3] + v[1]; planes[2][1] = v[7] + v[5]; planes[2][2] = v[11] + v[9]; planes[2][3] = v[15] + v[13];
	//left
	planes[3][0] = v[3] + v[0]; planes[3][1] = v[7] + v[4]; planes[3][2] = v[11] + v[8]; planes[3][3] = v[15] + v[12];
	//near
	planes[4][0] = v[3] + v[2]; planes[4][1] = v[7] + v[6]; planes[4][2] = v[11] + v[10]; planes[4][3] = v[15] + v[14];
	//far
	planes[5][0] = v[3] - v[2]; planes[5][1] = v[7] - v[6]; planes[5][2] = v[11] - v[10]; planes[5][3] = v[15] - v[14];

	//normalize to get correct distances
	for(int i = 0; i < 6; i++) {
		float *p = planes[i];
		float d = sqrt(p[0]*p[0] + p[1]*p[1] + p[2]*p[2]);
		for(int k = 0; k < 4; ++k) p[k] /= d;
	}
}

nx::Vector3f Frustum::viewPoint() {
	return view_point;
}
//how many pixel a unit length segment projects on the screen at point p
//or the closest point of a sphere if radius > 0
float Frustum::resolution(const nx::Vector3f &p, float radius) {
	float dist = p.dot(view_dir); //distance along the view axis
	dist -= radius * _scale;
	if(dist <= 0) return -1; //actually should return infinite...
	return _resolution * dist;
}

float Frustum::resolution(float dist) {
	return _resolution * dist;
}



bool Frustum::isOutside(const nx::Vector3f &center, float radius) {
	for(int i = 0; i < 4; i++) {
		float d = planeDistance(planes[i], center) + radius;
		if(d < 0) return true;
	}
	return false;
}

float Frustum::distance(const nx::Vector3f &center, float radius) {
	float distance = 1e20f;

	for(int i = 0; i < 4; i++) {
		float d = planeDistance(planes[i], center) + radius;
		if(d < distance) distance = d;
	}
	return distance; //usually negative
}


void Frustum::updateNearFar(float &znear, float &zfar, const nx::Vector3f &center, float radius) {
	float dist = (center - view_point).dot(view_dir); //distance along the view axis
	float dr = radius *_scale;
	if(znear > dist - dr) znear = dist - dr;
	if(zfar < dist + dr) zfar = dist + dr;
}

//(0, 0, -1, 1) * near_plane = zNear
//2zNear * zFar
//(zfar - znear)
void Frustum::updateNear(float &znear, const nx::Vector3f &p) {
	float dist = (p - view_point).dot(view_dir); //distance along the view axis
	if(znear > dist) znear = dist;
}
