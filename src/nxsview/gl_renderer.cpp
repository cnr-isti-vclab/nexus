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
#include "gl_renderer.h"

#include <cassert>
#include <cmath>

#include "../common/globalgl.h"
#include "../common/nexus.h"

using namespace nx;

void GLRenderer::getView() {
	float projection[16];
	float modelview[16];
	int viewport[4];
	glGetFloatv(GL_PROJECTION_MATRIX, projection);
	glGetFloatv(GL_MODELVIEW_MATRIX, modelview);
	glGetIntegerv(GL_VIEWPORT, viewport);
	Renderer::getView(projection, modelview, viewport);
}

void GLRenderer::beginRender(const RenderOptions &options) {
	if(options.draw_textures)
		glEnable(GL_TEXTURE_2D);
	if(options.draw_textures && !options.draw_texcoords) {
		glEnable(GL_TEXTURE_2D);
		glEnable(GL_TEXTURE_GEN_S);
		glEnable(GL_TEXTURE_GEN_T);
		glEnable(GL_TEXTURE_GEN_R);
		glEnable(GL_TEXTURE_GEN_Q);
	}

	glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
	glEnable(GL_COLOR_MATERIAL);
	glCheckError();

#ifdef GL_COMPATIBILITY
	glEnableClientState(GL_VERTEX_ARRAY);
	if(options.draw_colors) glEnableClientState(GL_COLOR_ARRAY);
	if(options.draw_normals) glEnableClientState(GL_NORMAL_ARRAY);
	if(options.draw_texcoords) glEnableClientState(GL_TEXTURE_COORD_ARRAY);
#endif

#if GL_ES || GL_CORE
	glEnableVertexAttribArray(ATTRIB_VERTEX);
	if(options.draw_colors) glEnableVertexAttribArray(ATTRIB_COLOR);
	if(options.draw_normals) glEnableVertexAttribArray(ATTRIB_NORMAL);
#endif
}

void GLRenderer::renderSelected(Nexus *nexus, const RenderOptions &options) {
	glCheckError();

	Signature &sig = nexus->header.signature;
	int GPU_loaded = 0;
	uint32_t last_texture = 0xffffffff;
	for(uint32_t i = 0; i <= last_node; i++) {
		if(!selected[i]) continue;

		Node &node = nexus->nodes[i];

		if(nexus->header.signature.face.hasIndex() && skipNode(i)) continue;

		stats.node_rendered++;

		nx::Sphere sphere = node.tightSphere();
		const nx::Vector3f &sphereCenter = sphere.Center();
		if(frustum_culling && metric.frustum.isOutside(sphereCenter, sphere.Radius())) {
			stats.frustum_culled++;
			continue;
		}
		nx::Vector3f viewPoint = metric.frustum.viewPoint();
		if(cone_culling && node.cone.Backface(sphere, viewPoint)) {
			stats.cone_culled++;
			continue;
		}
		glCheckError();
		if(0) {
			const nx::Vector3f &c = sphere.Center();
			float r = node.tight_radius;

			glLineWidth(4);
			glBegin(GL_LINE_STRIP);
			for(int i = 0; i < 21; i++)
				glVertex3f(c[0] + r*sin(i*M_PI/10), c[1] + r*cos(i*M_PI/10), c[2]);
			glEnd();
		}

		glCheckError();
		NodeData &data = nexus->nodedata[i];
		assert(data.memory);

		if(!data.vbo) {
			GPU_loaded++;
			nexus->loadGpu(i);
		}

		assert(data.vbo);

		glBindBuffer(GL_ARRAY_BUFFER, data.vbo);
		uint64_t start = 0;

#ifdef GL_COMPATIBILITY
		glVertexPointer(3, GL_FLOAT, 0, (void *)start);
		start += node.nvert*sig.vertex.attributes[VertexElement::COORD].size();

		if(options.draw_texcoords)
			glTexCoordPointer(2, GL_FLOAT, 0, (void *)start);
		if(sig.vertex.hasTextures())
			start += node.nvert * sig.vertex.attributes[VertexElement::TEX].size();

		if(options.draw_normals)
			glNormalPointer(GL_SHORT, 0, (void *)start);
		if(sig.vertex.hasNormals())
			start += node.nvert*sig.vertex.attributes[VertexElement::NORM].size();

		if(options.draw_colors)
			glColorPointer(4, GL_UNSIGNED_BYTE, 0, (void *)start);
		if(sig.vertex.hasColors())
			start += node.nvert * sig.vertex.attributes[VertexElement::COLOR].size();
#endif

#if GL_ES || GL_CORE
		glVertexAttribPointer(ATTRIB_VERTEX, 3, GL_FLOAT, GL_TRUE, 0, (void *)start);

		start += node.nvert*sig.vertex.attributes[VertexElement::COORD].size();

		if(options.draw_normals)
			glVertexAttribPointer(ATTRIB_NORMAL, 3, GL_SHORT, GL_TRUE, 0, (void *)start);
		if(sig.vertex.hasNormals())
			start += node.nvert*sig.vertex.attributes[VertexElement::NORM].size();

		if(options.draw_colors)
			glVertexAttribPointer(ATTRIB_COLOR, 4, GL_UNSIGNED_BYTE, GL_TRUE, 0, (void *)start);
		if(sig.vertex.hasColors())
			start += node.nvert * sig.vertex.attributes[VertexElement::COLOR].size();
#endif

		if(mode & PATCHES) {
			nx::Rgba8 color{};
			color[2] = ((i % 11)*171)%63 + 63;
			color[1] = ((i % 7)*57)%127 + 127;
			color[0] = ((i % 16)*135)%127 + 127;
#ifdef GL_COMPATIBILITY
			glColor3ub(color[0], color[1], color[2]);
#endif
#if GL_ES || GL_CORE
			glVertexAttrib4f(ATTRIB_COLOR, color[0]/255.0f, color[1]/255.0f, color[2]/255.0f, 1.0f);
#endif
		}

		glCheckError();

		if(!options.draw_triangles) {
			float pointsize = ceil(0.3*errors[i]);
			if(pointsize > 2) pointsize = 2;
			glPointSize(pointsize);

			glDrawArrays(GL_POINTS, 0, node.nvert);
			stats.patch_rendered++;
			stats.instance_rendered += node.nvert;
			continue;
		}

		if(options.draw_triangles)
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, data.fbo);

		uint32_t offset = 0;
		uint32_t end = 0;
		uint32_t last_patch = node.last_patch() - 1;
		for(uint32_t k = node.first_patch; k < node.last_patch(); k++) {
			Patch &patch = nexus->patches[k];

			if(!selected[patch.node]) {
				end = patch.triangle_offset;
				if(k != last_patch)
					continue;
			}
			if(end > offset) {
				if(options.draw_textures) {
					if(patch.texture != 0xffffffff) {
						if(last_texture != patch.texture) {
							if(!options.draw_texcoords) {
								Texture &texture = nexus->textures[patch.texture];
								glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
								glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
								glTexGeni(GL_R, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
								glTexGeni(GL_Q, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);

								glTexGenfv(GL_S, GL_OBJECT_PLANE, &texture.matrix[0]);
								glTexGenfv(GL_T, GL_OBJECT_PLANE, &texture.matrix[4]);
								glTexGenfv(GL_R, GL_OBJECT_PLANE, &texture.matrix[8]);
								glTexGenfv(GL_Q, GL_OBJECT_PLANE, &texture.matrix[12]);
							}
							TextureData &tdata = nexus->texturedata[patch.texture];
							glBindTexture(GL_TEXTURE_2D, tdata.tex);
							last_texture = patch.texture;
						}
					} else {
						glBindTexture(GL_TEXTURE_2D, 0);
						last_texture = 0xffffffff;
					}
				}

				if(!options.draw_triangles) {
					if(nexus->header.signature.face.hasIndex())
						glDrawArrays(GL_POINTS, 0, node.nvert);
					else
						glDrawArrays(GL_POINTS, offset, end - offset);
				} else {
					glDrawElements(GL_TRIANGLES, (end - offset)*3, GL_UNSIGNED_SHORT, (void *)(offset*3*sizeof(uint16_t)));
				}
				stats.patch_rendered++;
				stats.instance_rendered += end - offset;
			}
			offset = patch.triangle_offset;
		}
		glCheckError();
	}
}

void GLRenderer::endRender(const RenderOptions &options) {
	glCheckError();

#ifdef GL_COMPATIBILITY
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	if(options.draw_triangles) glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	glDisableClientState(GL_VERTEX_ARRAY);
	if(options.draw_normals) glDisableClientState(GL_NORMAL_ARRAY);
	if(options.draw_colors) glDisableClientState(GL_COLOR_ARRAY);
	if(options.draw_texcoords) glDisableClientState(GL_TEXTURE_COORD_ARRAY);
#endif

#if GL_ES || GL_CORE
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	if(options.draw_triangles) glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	glDisableVertexAttribArray(ATTRIB_VERTEX);
	if(options.draw_normals) glDisableVertexAttribArray(ATTRIB_NORMAL);
	if(options.draw_colors) glDisableVertexAttribArray(ATTRIB_COLOR);
#endif

	if(options.draw_textures)
		glDisable(GL_TEXTURE_2D);
	if(options.draw_textures && !options.draw_texcoords) {
		glDisable(GL_TEXTURE_GEN_S);
		glDisable(GL_TEXTURE_GEN_T);
		glDisable(GL_TEXTURE_GEN_R);
		glDisable(GL_TEXTURE_GEN_Q);
	}
	glDisable(GL_COLOR_MATERIAL);
}
