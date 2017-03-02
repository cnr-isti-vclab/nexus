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
#ifndef NX_TOKEN_H
#define NX_TOKEN_H

#include <wrap/gcache/token.h>

namespace nx {

class Nexus;

class Priority {
public:
	float error;
	uint32_t frame;
	Priority(float e = 0, uint32_t f = 0): error(e), frame(f) {}
	bool operator<(const Priority &p) const {
		if(frame == p.frame) return error < p.error;
		return frame < p.frame;
	}
	bool operator>(const Priority &p) const {
		if(frame == p.frame) return error > p.error;
		return frame > p.frame;
	}
};

class Token: public vcg::Token<Priority> {
public:
	Nexus *nexus;
	uint32_t node;

	Token() {}
	Token(Nexus *nx, uint32_t n):  nexus(nx), node(n) {}
};

} //namespace
#endif // NX_TOKEN_H
