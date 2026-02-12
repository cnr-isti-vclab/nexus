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

#ifndef __RASTERIZER_H__
#define __RASTERIZER_H__

#include <vector>

#include "../core/material.h"
#include "../core/mesh_types.h"
#include "texture_cache.h"

namespace nx {

class Edge {
public:
    Vector2f uv1;
    Vector2f uv2;
    int X1, Y1, X2, Y2;

    Edge(const Vector2f& uv1, int x1, int y1, const Vector2f& uv2, int x2, int y2);
};

class Span {
public:
    Vector2f uv1;
    Vector2f uv2;
    int X1, X2;

    Span(const Vector2f& uv1, int x1, const Vector2f& uv2, int x2);
};

class Rasterizer {
protected:
    std::vector<Vector3f>& target;
    int width;
    int height;
    const std::vector<Material>* materials = nullptr;
    TextureCache* texture_cache = nullptr;
    Rasterizer(std::vector<Vector3f>& buffer, int w, int h): target(buffer), width(w), height(h) {}

    void DrawSpan(const Span &span, int y, Index material_id);
    void DrawSpansBetweenEdges(const Edge &e1, const Edge &e2, Index material_id);

public:
    void SetPixel(unsigned int x, unsigned int y, Index material_id, const Vector2f& uv);
    void SetPixel(int x, int y, Index material_id, const Vector2f& uv);
    void SetPixel(float x, float y, Index material_id, const Vector2f& uv);

    void DrawTriangle(Index material_id,
        float x1, float y1, const Vector2f& uv1,
        float x2, float y2, const Vector2f& uv2,
        float x3, float y3, const Vector2f& uv3);

    void DrawLine(Index material_id,
        float x1, float y1, const Vector2f& uv1,
        float x2, float y2, const Vector2f& uv2);

    void RasterizeTriangles(const std::vector<Vector2f>& positions,
        const std::vector<Vector2f>& uvs,
        const std::vector<Index>& material_ids,
        const std::vector<Material>& materials,
        TextureCache* texture_cache);
};

}

#endif /* __RASTERIZER_H__ */
