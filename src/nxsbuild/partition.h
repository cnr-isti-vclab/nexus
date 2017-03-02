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
#ifndef NX_PARTITION_H
#define NX_PARTITION_H

#include <vcg/space/point3.h>
#include "trianglesoup.h"
#include <vector>

class StreamSoup;

class Partition: public VirtualTriangleSoup {
public:
	Partition(QString prefix): VirtualTriangleSoup(prefix) {}
	virtual ~Partition() {}
	virtual void load(StreamSoup &stream) = 0;
	virtual bool isInNode(quint32 node, vcg::Point3f &p) = 0;
};

#endif // NX_PARTITION_H
