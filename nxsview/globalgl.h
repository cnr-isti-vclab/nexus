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
#ifndef NX_GLOBALGL_H
#define NX_GLOBALGL_H

#ifdef GL_ES

#include <OpenGLES/ES2/gl.h>

#else

#include <GL/glew.h>

#endif

#include <string>
#include <iostream>

void _glCheckError(const char *file, int line);
#define glCheckError() _glCheckError(__FILE__,__LINE__)

#endif // NX_GLOBALGL_H
