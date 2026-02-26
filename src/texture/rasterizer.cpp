/*
 * Copyright (C) 2009 Josh A. Beam
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <cstdint>
#include "rasterizer.h"

namespace nx {

Edge::Edge(const Vector2f& uv1_, int x1, int y1,
		   const Vector2f& uv2_, int x2, int y2) {
	if(y1 < y2) {
		uv1 = uv1_;
		X1 = x1;
		Y1 = y1;
		uv2 = uv2_;
		X2 = x2;
		Y2 = y2;
	} else {
		uv1 = uv2_;
		X1 = x2;
		Y1 = y2;
		uv2 = uv1_;
		X2 = x1;
		Y2 = y1;
	}
}

Span::Span(const Vector2f& uv1_, int x1, const Vector2f& uv2_, int x2) {
	if(x1 < x2) {
		uv1 = uv1_;
		X1 = x1;
		uv2 = uv2_;
		X2 = x2;
	} else {
		uv1 = uv2_;
		X1 = x2;
		uv2 = uv1_;
		X2 = x1;
	}
}

void Rasterizer::SetPixel(unsigned int x, unsigned int y, Index material_id, const Vector2f& uv) {
	if(x >= static_cast<unsigned int>(width) || y >= static_cast<unsigned int>(height))
		return;
	assert(materials != nullptr);
	assert(texture_cache != nullptr);
	assert(tilemap != nullptr);
	assert(tilemap->width == width);
	assert(tilemap->height == height);
	assert(material_id < materials->size());
	if(write_mask) {
		(*write_mask)[static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)] = 1;
	}

	const Material& material = (*materials)[material_id];
	const float u = std::clamp(uv.u, 0.0f, 1.0f);
	const float v = std::clamp(uv.v, 0.0f, 1.0f);
	auto to_u8 = [](float value) -> uint8_t {
		float clamped = std::clamp(value, 0.0f, 1.0f);
		return static_cast<uint8_t>(clamped * 255.0f + 0.5f);
	};
	for (Material::TextureSlot slot : tilemap->texture_slots) {
		size_t slot_index = Material::slotIndex(slot);
		uint8_t* pixel = tilemap->pixel(static_cast<int>(x), static_cast<int>(y), slot);

		Vector3f tex{1.0f, 1.0f, 1.0f};
		Material::TextureId tex_id = material.texture_ids[slot_index];
		if (tex_id != Material::INVALID_TEXTURE_ID) {
			//sampling highest resolution levels
			tex = texture_cache->sample(tex_id, {u, v});
		}

		if (slot == Material::TextureSlot::BaseColor) {
			tex.x *= material.base_color[0];
			tex.y *= material.base_color[1];
			tex.z *= material.base_color[2];
		}

		pixel[0] = to_u8(tex.x);
		pixel[1] = to_u8(tex.y);
		pixel[2] = to_u8(tex.z);
	}
}

void Rasterizer::SetPixel(int x, int y, Index material_id, const Vector2f& uv) {
	if(x < 0 || y < 0)
		return;
	SetPixel(static_cast<unsigned int>(x), static_cast<unsigned int>(y), material_id, uv);
}

void Rasterizer::SetPixel(float x, float y, Index material_id, const Vector2f& uv) {
	if(x < 0.0f || y < 0.0f)
		return;

	SetPixel(static_cast<unsigned int>(x), static_cast<unsigned int>(y), material_id, uv);
}

void Rasterizer::DrawSpan(const Span &span, int y, Index material_id) {
	int xdiff = span.X2 - span.X1;
	if(xdiff == 0)
		return;

	Vector2f diff{span.uv2.u - span.uv1.u, span.uv2.v - span.uv1.v};

	float factor = 0.0f;
	float factorStep = 1.0f / static_cast<float>(xdiff);

	// draw each pixel in the span
	for(int x = span.X1; x < span.X2; x++) {
		Vector2f uv{span.uv1.u + (diff.u * factor), span.uv1.v + (diff.v * factor)};
		SetPixel(x, y, material_id, uv);
		factor += factorStep;
	}
}

void Rasterizer::DrawSpansBetweenEdges(const Edge &e1, const Edge &e2, Index material_id) {
	// calculate difference between the y coordinates
	// of the first edge and return if 0
	float e1ydiff = static_cast<float>(e1.Y2 - e1.Y1);
	if(e1ydiff == 0.0f)
		return;

	// calculate difference between the y coordinates
	// of the second edge and return if 0
	float e2ydiff = static_cast<float>(e2.Y2 - e2.Y1);
	if(e2ydiff == 0.0f)
		return;

	// calculate differences between the x coordinates
	// and colors of the points of the edges
	float e1xdiff = static_cast<float>(e1.X2 - e1.X1);
	float e2xdiff = static_cast<float>(e2.X2 - e2.X1);
	Vector2f e1uvdiff{e1.uv2.u - e1.uv1.u, e1.uv2.v - e1.uv1.v};
	Vector2f e2uvdiff{e2.uv2.u - e2.uv1.u, e2.uv2.v - e2.uv1.v};

	// calculate factors to use for interpolation
	// with the edges and the step values to increase
	// them by after drawing each span
	float factor1 = static_cast<float>(e2.Y1 - e1.Y1) / e1ydiff;
	float factorStep1 = 1.0f / e1ydiff;
	float factor2 = 0.0f;
	float factorStep2 = 1.0f / e2ydiff;

	// loop through the lines between the edges and draw spans
	for(int y = e2.Y1; y < e2.Y2; y++) {
		// create and draw span
		Span span(
			Vector2f{e1.uv1.u + (e1uvdiff.u * factor1), e1.uv1.v + (e1uvdiff.v * factor1)},
			e1.X1 + static_cast<int>(e1xdiff * factor1),
			Vector2f{e2.uv1.u + (e2uvdiff.u * factor2), e2.uv1.v + (e2uvdiff.v * factor2)},
			e2.X1 + static_cast<int>(e2xdiff * factor2));
		DrawSpan(span, y, material_id);

		// increase factors
		factor1 += factorStep1;
		factor2 += factorStep2;
	}
}

void
Rasterizer::DrawTriangle(Index material_id,
						 float x1, float y1, const Vector2f& uv1,
						 float x2, float y2, const Vector2f& uv2,
						 float x3, float y3, const Vector2f& uv3)
{
	// create edges for the triangle
	Edge edges[3] = {
		Edge(uv1, static_cast<int>(x1), static_cast<int>(y1), uv2, static_cast<int>(x2), static_cast<int>(y2)),
		Edge(uv2, static_cast<int>(x2), static_cast<int>(y2), uv3, static_cast<int>(x3), static_cast<int>(y3)),
		Edge(uv3, static_cast<int>(x3), static_cast<int>(y3), uv1, static_cast<int>(x1), static_cast<int>(y1))
	};

	int maxLength = 0;
	int longEdge = 0;

	// find edge with the greatest length in the y axis
	for(int i = 0; i < 3; i++) {
		int length = edges[i].Y2 - edges[i].Y1;
		if(length > maxLength) {
			maxLength = length;
			longEdge = i;
		}
	}

	int shortEdge1 = (longEdge + 1) % 3;
	int shortEdge2 = (longEdge + 2) % 3;

	// draw spans between edges; the long edge can be drawn
	// with the shorter edges to draw the full triangle
	DrawSpansBetweenEdges(edges[longEdge], edges[shortEdge1], material_id);
	DrawSpansBetweenEdges(edges[longEdge], edges[shortEdge2], material_id);
}

void
Rasterizer::DrawLine(Index material_id,
					 float x1, float y1, const Vector2f& uv1,
					 float x2, float y2, const Vector2f& uv2)
{
	float xdiff = (x2 - x1);
	float ydiff = (y2 - y1);

	if(xdiff == 0.0f && ydiff == 0.0f) {
		SetPixel(x1, y1, material_id, uv1);
		return;
	}

	if(fabs(xdiff) > fabs(ydiff)) {
		float xmin, xmax;

		// set xmin to the lower x value given
		// and xmax to the higher value
		if(x1 < x2) {
			xmin = x1;
			xmax = x2;
		} else {
			xmin = x2;
			xmax = x1;
		}

		// draw line in terms of y slope
		float slope = ydiff / xdiff;
		for(float x = xmin; x <= xmax; x += 1.0f) {
			float y = y1 + ((x - x1) * slope);
			float t = (x - x1) / xdiff;
			Vector2f uv{uv1.u + (uv2.u - uv1.u) * t, uv1.v + (uv2.v - uv1.v) * t};
			SetPixel(x, y, material_id, uv);
		}
	} else {
		float ymin, ymax;

		// set ymin to the lower y value given
		// and ymax to the higher value
		if(y1 < y2) {
			ymin = y1;
			ymax = y2;
		} else {
			ymin = y2;
			ymax = y1;
		}

		// draw line in terms of x slope
		float slope = xdiff / ydiff;
		for(float y = ymin; y <= ymax; y += 1.0f) {
			float x = x1 + ((y - y1) * slope);
			float t = (y - y1) / ydiff;
			Vector2f uv{uv1.u + (uv2.u - uv1.u) * t, uv1.v + (uv2.v - uv1.v) * t};
			SetPixel(x, y, material_id, uv);
		}
	}
}

std::vector<uint8_t> Rasterizer::rasterizeTriangles(
	const std::vector<Vector2f>& positions,
	const std::vector<Vector2f>& uvs,
	const std::vector<Index>& material_ids,
	const std::vector<Material>& materials_in,
	TextureCache* texture_cache_in,
	TileMap* tilemap_in) {

	assert(positions.size() == uvs.size());
	assert(positions.size() % 3 == 0);
	assert(material_ids.size() == positions.size() / 3);
	assert(tilemap_in != nullptr);
	assert(tilemap_in->width == width);
	assert(tilemap_in->height == height);
	assert(!tilemap_in->texture_slots.empty());
	std::vector<uint8_t> mask(static_cast<size_t>(width) * static_cast<size_t>(height), 0);

	materials = &materials_in;
	texture_cache = texture_cache_in;
	tilemap = tilemap_in;
	write_mask = &mask;

	const size_t triangle_count = material_ids.size();
	for(size_t i = 0; i < triangle_count; ++i) {
		const Vector2f& p0 = positions[i * 3 + 0];
		const Vector2f& p1 = positions[i * 3 + 1];
		const Vector2f& p2 = positions[i * 3 + 2];
		const Vector2f& uv0 = uvs[i * 3 + 0];
		const Vector2f& uv1 = uvs[i * 3 + 1];
		const Vector2f& uv2 = uvs[i * 3 + 2];
		const Index material_id = material_ids[i];
		DrawTriangle(material_id, p0.u, p0.v, uv0, p1.u, p1.v, uv1, p2.u, p2.v, uv2);
	}
	materials = nullptr;
	texture_cache = nullptr;
	tilemap = nullptr;
	write_mask = nullptr;
	return mask;
}



}
