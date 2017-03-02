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

#ifndef NX_NORMALS_CONE_H
#define NX_NORMALS_CONE_H

#include <vector>

#include <vcg/space/point3.h>
#include <vcg/space/sphere3.h>

namespace nx {


//anchored normal cone.
class AnchoredCone3f {
public:
	AnchoredCone3f();

	bool Frontface(const vcg::Point3f &viewPoint);
	bool Backface(const vcg::Point3f &viewPoint);

	//threshold [0, 1]: percentage of normals to left out
	void AddNormals(std::vector<vcg::Point3f> &normals,
					float threshold = 1.0f);
	void AddNormals(std::vector<vcg::Point3f> &normals,
					std::vector<float> &areas,
					float threshold = 1.0f);
	void AddAnchors(std::vector<vcg::Point3f> &anchors);

protected:
	vcg::Point3f scaledNormal;
	vcg::Point3f frontAnchor;
	vcg::Point3f backAnchor;

	friend class Cone3s;
};



//this is not anchored... need a bounding sphere to report something
class Cone3s {
public:
	void Import(const AnchoredCone3f &c);

	bool Backface(const vcg::Sphere3f &sphere,
				  const vcg::Point3f &view) const;
	bool Frontface(const vcg::Sphere3f &sphere,
				   const vcg::Point3f &view) const;

	//encode normal and sin(alpha) in n[3]
	short n[4];
};

}

#endif
