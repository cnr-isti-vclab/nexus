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
#ifndef NX_GL_RENDERER_H
#define NX_GL_RENDERER_H

#include "../../renderer.h"

namespace nx {

class GLRenderer : public Renderer {
public:
	using Renderer::getView;
	void getView() override;

protected:
	void beginRender(const RenderOptions &options) override;
	void renderSelected(Nexus *nexus, const RenderOptions &options) override;
	void endRender(const RenderOptions &options) override;
};

} // namespace nx

#endif // NX_GL_RENDERER_H
