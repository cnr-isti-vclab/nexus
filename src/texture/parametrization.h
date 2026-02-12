#pragma once

#include <cstdint>

#include "../nxsbuild/merge_simplify_split.h"
#include "../core/material.h"

namespace nx {

struct ParametrizationOptions {
	uint32_t padding = 2;
	float texels_per_unit = 1.0f;
	uint32_t resolution = 128;
	bool use_bruteforce = true;
	bool block_align = false; //set true for compressed textures, false for better packing
	bool rotate_charts = true;
	bool rotate_charts_to_axis = true;
	float max_chart_area = 0.0f;
	float max_boundary_length = 0.0f;
	float normal_deviation_weight = 2.0f;
	float roundness_weight = 0.01f;
	float straightness_weight = 6.0f;
	float normal_seam_weight = 4.0f;
	float texture_seam_weight = 0.5f;
	float max_cost = 4.0f;
	uint32_t max_iterations = 4;
};

// Generates a new UV parametrization for a merged mesh using xatlas.
// If out_texcoords is provided, it will be filled with the generated UVs indexed by wedge.t.
// Returns true on success, false on failure (mesh is left with best-effort UVs).
bool create_parametrization(NodeMesh& mesh,
	const ParametrizationOptions& options = {},
	std::vector<Vector2f>* out_texcoords = nullptr);

TileMap rasterize(std::vector<Material> &materials, NodeMesh &source, NodeMesh &destination, int tex_res);
void reparametrize_clusters(MappedMesh& mesh);

} // namespace nx
