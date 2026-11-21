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
#ifndef NX_MATH_H
#define NX_MATH_H

#include <stdint.h>
#include <math.h>

class Math {
public:
	static int log2(uint32_t v) {

#ifdef USE_DOUBLE_LOG2
		int r; // result of log_2(v) goes here
		union { unsigned int u[2]; double d; } t; // temp

		t.u[__FLOAT_WORD_ORDER==LITTLE_ENDIAN] = 0x43300000;
		t.u[__FLOAT_WORD_ORDER!=LITTLE_ENDIAN] = v;
		t.d -= 4503599627370496.0;
		r = (t.u[__FLOAT_WORD_ORDER==LITTLE_ENDIAN] >> 20) - 0x3FF;
		return r;

#else

		static const char LogTable256[256] = {
	#define LT(n) n, n, n, n, n, n, n, n, n, n, n, n, n, n, n, n
			-1, 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3,
			LT(4), LT(5), LT(5), LT(6), LT(6), LT(6), LT(6),
			LT(7), LT(7), LT(7), LT(7), LT(7), LT(7), LT(7), LT(7)
		};

		uint32_t r;     // r will be lg(v)
		uint32_t t, tt; // temporaries

		if (tt = v >> 16) {
			r = (t = tt >> 8) ? 24 + LogTable256[t] : 16 + LogTable256[tt];
		} else  {
			r = (t = v >> 8) ? 8 + LogTable256[t] : LogTable256[v];
		}
		return r;
#endif
	}
	static int ilogbf(float r) {
		return floor(log(r)/log(2.0));
		//THe following is a unknown mistery:
		//if cout and assert are uncommented everything works,
		//otherwise bogus values are reported. WHY?
		//valgrind produces strange errors...

		unsigned int n = *(int *)&r;
		n <<= 1;
		n >>= 24;
		int s = ((int)n) - 127;
		//cout << s << " vs: " << ::logbf(r) << endl;
		//assert(s == ::logbf(r));
		return s;
	}
};

#endif // NX_MATH_H
