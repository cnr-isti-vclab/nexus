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
#ifndef NX_FPU_PRECISION_H
#define NX_FPU_PRECISION_H

#ifdef _GNUCC
#include <fpu_control.h>



class FpuPrecision {
public:
	static void store() {
		_FPU_GETCW(old()); // store old cw
	}
	static void setFloat() {
		fpu_control_t fpu_cw = (old() & ~_FPU_EXTENDED & ~_FPU_DOUBLE & ~_FPU_SINGLE) | _FPU_SINGLE;
		_FPU_SETCW(fpu_cw);
	}

	static void setDouble() {
		fpu_control_t fpu_cw = (old() & ~_FPU_EXTENDED & ~_FPU_DOUBLE & ~_FPU_SINGLE) | _FPU_DOUBLE;
		_FPU_SETCW(fpu_cw);
	}

	static void restore() {
		_FPU_SETCW(old()); // restore old cw
	}

	static fpu_control_t &old() {
		static fpu_control_t f;
		return f;
	}
};

#else
class FpuPrecision {
public:
	static void store() {

	}
	static void setFloat() {

	}

	static void setDouble() {

	}

	static void restore() {

	}


};
#endif

#endif // NX_FPU_PRECISION_H
