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
#ifndef NX_TSPLOADER_H
#define NX_TSPLOADER_H

#include "meshloader.h"

class TspLoader: public MeshLoader {
public:
	TspLoader(QString file);
	void setMaxMemory(quint64 /*m*/) {  }
	quint32 getTriangles(quint32 size, Triangle *buffer);
	quint32 getVertices(quint32 /*size*/, Splat */*vertex*/) { assert(0); return 0; }

private:
	QFile file;
	quint64 current_triangle;

};

#endif // NX_TSPLOADER_H
