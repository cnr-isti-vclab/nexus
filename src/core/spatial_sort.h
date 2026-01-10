#pragma once

#include "mesh.h"
#include <vector>

namespace nx {

// TODO: In the future we will use mmapped files for out-of-core sorting.
// In that case we should sort in place the vertices and the indices at the same time.

// Compute Morton code (Z-order) for a 3D point in normalized [0,1]^3 space
uint64_t morton_code(float x, float y, float z);

// Sort positions using Z-order space-filling curve
// Returns a remapping array where remap[old_index] = new_index
std::vector<Index> spatial_sort_positions(MeshFiles& mesh);

// Apply remapping to wedge position indices
void remap_wedge_positions(MeshFiles& mesh, const std::vector<Index>& remap);

// Sort wedges by their position index
// Returns a remapping array where remap[old_wedge_index] = new_wedge_index
std::vector<Index> sort_wedges_by_position(MeshFiles& mesh);

// Apply remapping to triangle wedge indices
void remap_triangle_wedges(MeshFiles& mesh, const std::vector<Index>& remap);

// Sort triangles by their smallest wedge index
void sort_triangles_by_wedge(MeshFiles& mesh);

// Complete spatial sorting pipeline:
// 1. Sort positions by Z-order curve
// 2. Sort wedges by position index
// 3. Sort triangles by smallest wedge index
void spatial_sort_mesh(MeshFiles& mesh);

void spatial_sort_triangles(MeshFiles& mesh);

} // namespace nx
