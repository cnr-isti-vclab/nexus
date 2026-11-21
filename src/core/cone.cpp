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
#include <iostream>
#include <algorithm>
#include <cmath>
#include "cone.h"

#include <assert.h>

using namespace std;
using namespace nx;

AnchoredCone3f::AnchoredCone3f() {
	scaledNormal = {0, 0, 0};
	frontAnchor = {0, 0, 0};
	backAnchor = {0, 0, 0};
}

bool AnchoredCone3f::Frontface(const Vector3f &viewPoint) {
	Vector3f d = frontAnchor - viewPoint; //Vector from viewPoint to frontAnchor.
	float f = -d.dot(scaledNormal);
	if (f < 0.001 || f * f < d.dot(d))
		return false;
	return true;
}

bool AnchoredCone3f::Backface(const Vector3f &viewPoint) {
	Vector3f d = backAnchor - viewPoint; //Vector from viewPoint to frontAnchor.
	float f = d.dot(scaledNormal);
	if (f < 0.001 || f * f < d.dot(d))
		return false;
	return true;
}

static float vectorAngle(const Vector3f &a, const Vector3f &b) {
	float denominator = a.norm() * b.norm();
	if(denominator <= 0) return 0;
	float cosine = a.dot(b) / denominator;
	return std::acos(std::max(-1.0f, std::min(1.0f, cosine)));
}

void AnchoredCone3f::AddNormals(vector<Vector3f> &normal, vector<float> &area, float threshold) {
	assert(normal.size() == area.size());
	//compute average normal
	scaledNormal = {0, 0, 0};
	for(auto &n: normal)
		scaledNormal += n;
	float averageLength = scaledNormal.norm();
	if(averageLength > 0) scaledNormal /= averageLength;

	//create 50 concentric rings around the average normal (scaledNormal)
	double distr[50];
	for(int k = 0; k < 50; k++)
		distr[k] = 0;
	double tot_area = 0;

	vector<float>::iterator j;
	vector<Vector3f>::iterator i;
	for(i = normal.begin(), j = area.begin(); i != normal.end(); i++, j++) {
		int pos = (int)(49.0 * vectorAngle(scaledNormal, *i)/M_PI);
		if(pos < 0) continue;
		assert(pos >=0 && pos < 50);
		distr[pos] += *j;
		tot_area += *j;
	}

	float tot = 0;
	int best;
	for(best = 0; best < 50; best++) {
		tot += (float)distr[best];
		if(tot > threshold * tot_area)
			break;
	}
	double alpha = M_PI * (best + 1) / 50;
	if(alpha >= M_PI/ 2 - 0.1)
		scaledNormal = {0, 0, 0};
	else
		scaledNormal /= std::cos(M_PI/2.0f - alpha);
}

void AnchoredCone3f::AddNormals(vector<Vector3f> &normal, float threshold) {
	assert(normal.size() > 0);
	scaledNormal = {0, 0, 0};
	int count = 0;
	for(auto &n: normal) {
		float length = n.norm();
		if(length < 0.00001f) continue;
		n /= length;
		scaledNormal += n;
		count++;
	}
	if(count == 0) return;
	scaledNormal /= count;
	float len = scaledNormal.norm();
	if(len == 0) return;
	scaledNormal /= len;

	int distr[50];
	for(int k = 0; k < 50; k++)
		distr[k] =0;

	for(auto &n: normal) {
		int pos = (int)(49.0 * vectorAngle(scaledNormal, n)/M_PI);
		pos = std::max(0, std::min(49, pos));
		distr[pos]++;
	}
	int tot = 0;
	int best;
	for(best = 0; best < 50; best++) {
		tot += distr[best];
		if(tot >= threshold * normal.size())
			break;
	}
	double alpha = M_PI * (best +1) / 50;
	if(alpha >= M_PI/ 2 - 0.1) {
		scaledNormal = {0, 0, 0};
	} else {
		scaledNormal /= std::cos(M_PI/2.0f - alpha);
	}
}

void AnchoredCone3f::AddAnchors(vector<Vector3f> &anchors) {
	assert(anchors.size() > 0);
	frontAnchor = anchors[0];
	backAnchor = anchors[0];

	float fa = frontAnchor.dot(scaledNormal);
	float fb = -backAnchor.dot(scaledNormal);

	vector<Vector3f>::iterator i;
	for(i = anchors.begin(); i != anchors.end(); i++) {
		Vector3f &anchor = *i;
		float na = anchor.dot(scaledNormal);
		if(na < fa) {
			frontAnchor = anchor;
			fa = na;
		}
		if(-na < fb) {
			backAnchor = anchor;
			fb = -na;
		}
	}
}

void Cone3s::Import(const AnchoredCone3f &c) {
	Vector3f normal = c.scaledNormal;
	float len = normal.norm();
	if(len > 0.001)
		normal /= len;

	for(int i = 0; i < 3; i++) {
		assert(normal[i] < 1.01 && normal[i] > -1.01);

		if(normal[i] > 1.0f) normal[i] = 1;
		if(normal[i] < -1.0f) normal[i] = -1;
	}
	n[0] = (short)(normal[0] * 32766);
	n[1] = (short)(normal[1] * 32766);
	n[2] = (short)(normal[2] * 32766);
	//i want to represent numbers from -10 to 10
	if(len > 10.0f) len = 10.0f;
	if(len < -10.0f) len = -10.0f;
	n[3] = (short)(len * 3276);
}


bool Cone3s::Backface(const nx::Sphere &sphere,
					  const nx::Vector3f &view) const {
	nx::Vector3f norm{n[0]/32766.0f, n[1]/32766.0f, n[2]/32766.0f};
	nx::Vector3f d = (sphere.Center() - norm * sphere.Radius()) - view;
	norm *= n[3]/32766.0f;

	float f = d.dot(norm);
	if (f < 0.001 || f * f < d.dot(d))
		return false;
	return true;
}

bool Cone3s::Frontface(const nx::Sphere &sphere,
					   const nx::Vector3f &view) const {
	nx::Vector3f norm{n[0]/32766.0f, n[1]/32766.0f, n[2]/32766.0f};
	nx::Vector3f d = (sphere.Center() + norm * sphere.Radius()) - view;
	norm *= n[3]/3276.0f;

	float f = -d.dot(norm);
	if (f < 0.001 || f * f < d.dot(d))
		return false;
	return true;
}
