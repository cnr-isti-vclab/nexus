#include "parametrization.h"

#include "xatlas.h"

#include <cmath>
#include <vector>
#include <iostream>
#include <unordered_map>

namespace nx {

namespace {

void ParameterizeCharts(xatlas::Atlas* atlas) {
	(void)atlas;
	// Parameterization is handled internally by xatlas during ComputeCharts.
}

} // namespace

bool create_parametrization(MergedMesh& mesh, const ParametrizationOptions& options, std::vector<Vector2f>* out_texcoords) {
	if (mesh.positions.empty() || mesh.triangles.empty()) {
		return false;
	}

	xatlas::Atlas* atlas = xatlas::Create();
	if (!atlas) {
		return false;
	}

	std::vector<Index> indices;
	indices.reserve(mesh.triangles.size() * 3);
	for (const Triangle& tri : mesh.triangles) {
		for (int k = 0; k < 3; ++k) {
			Index wedge_idx = tri.w[k];
			indices.push_back(mesh.wedges[wedge_idx].p);
		}
	}

	xatlas::MeshDecl mesh_decl{};
	mesh_decl.vertexCount = static_cast<uint32_t>(mesh.positions.size());
	mesh_decl.vertexPositionData = mesh.positions.data();
	mesh_decl.vertexPositionStride = sizeof(Vector3f);
	mesh_decl.indexCount = static_cast<uint32_t>(indices.size());
	mesh_decl.indexData = indices.data();
	mesh_decl.indexFormat = xatlas::IndexFormat::UInt32;
	mesh_decl.faceCount = static_cast<uint32_t>(mesh.triangles.size());

	xatlas::AddMeshError add_error = xatlas::AddMesh(atlas, mesh_decl);
	if (add_error != xatlas::AddMeshError::Success) {
		std::cerr << "xatlas AddMesh failed: " << xatlas::StringForEnum(add_error) << std::endl;
		xatlas::Destroy(atlas);
		return false;
	}

	xatlas::ChartOptions chart_options{};
	chart_options.maxChartArea = options.max_chart_area;
	chart_options.maxBoundaryLength = options.max_boundary_length;
	chart_options.normalDeviationWeight = options.normal_deviation_weight;
	chart_options.roundnessWeight = options.roundness_weight;
	chart_options.straightnessWeight = options.straightness_weight;
	chart_options.normalSeamWeight = options.normal_seam_weight;
	chart_options.textureSeamWeight = options.texture_seam_weight;
	chart_options.maxCost = options.max_cost;
	chart_options.maxIterations = options.max_iterations;
	chart_options.useInputMeshUvs = false;
	xatlas::PackOptions pack_options{};
	pack_options.padding = options.padding;
	pack_options.resolution = options.resolution;
	pack_options.texelsPerUnit = options.resolution > 0 ? 0.0f : options.texels_per_unit;
	pack_options.bruteForce = options.use_bruteforce;
	pack_options.blockAlign = options.block_align;
	pack_options.rotateCharts = options.rotate_charts;
	pack_options.rotateChartsToAxis = options.rotate_charts_to_axis;

	xatlas::ComputeCharts(atlas, chart_options);
	ParameterizeCharts(atlas);
	xatlas::PackCharts(atlas, pack_options);

	if (atlas->meshCount == 0 || atlas->meshes == nullptr) {
		xatlas::Destroy(atlas);
		return false;
	}

	xatlas::Mesh& out_mesh = atlas->meshes[0];
	if (out_mesh.indexCount != indices.size()) {
		xatlas::Destroy(atlas);
		return false;
	}

	float inv_width = atlas->width > 0 ? 1.0f / static_cast<float>(atlas->width) : 1.0f;
	float inv_height = atlas->height > 0 ? 1.0f / static_cast<float>(atlas->height) : 1.0f;

	// xatlas returns per-corner UVs. Rebuild wedges per triangle, then unify by position/normal/UV.
	std::vector<Wedge> new_wedges;
	new_wedges.reserve(mesh.triangles.size() * 3);
	std::vector<Triangle> new_triangles(mesh.triangles.size());
	std::vector<Vector2f> texcoords;
	texcoords.reserve(mesh.triangles.size() * 3);

	struct WedgeKey {
		Index p;
		Index n;
		int32_t u;
		int32_t v;
		bool operator==(const WedgeKey& other) const {
			return p == other.p && n == other.n && u == other.u && v == other.v;
		}
	};
	struct WedgeKeyHash {
		std::size_t operator()(const WedgeKey& k) const {
			std::size_t h = std::hash<Index>{}(k.p);
			h ^= std::hash<Index>{}(k.n) + 0x9e3779b9 + (h << 6) + (h >> 2);
			h ^= std::hash<int32_t>{}(k.u) + 0x9e3779b9 + (h << 6) + (h >> 2);
			h ^= std::hash<int32_t>{}(k.v) + 0x9e3779b9 + (h << 6) + (h >> 2);
			return h;
		}
	};
	std::unordered_map<WedgeKey, Index, WedgeKeyHash> wedge_map;
	const float quant = 1e6f;

	for (std::size_t i = 0; i < mesh.triangles.size(); ++i) {
		const Triangle& tri = mesh.triangles[i];
		Triangle new_tri{};
		std::size_t base = i * 3;
		for (int k = 0; k < 3; ++k) {
			Index wedge_idx = tri.w[k];
			uint32_t out_index = out_mesh.indexArray[base + k];
			if (out_index >= out_mesh.vertexCount) {
				new_tri.w[k] = wedge_idx;
				continue;
			}
			const xatlas::Vertex& v = out_mesh.vertexArray[out_index];
			Vector2f uv{v.uv[0] * inv_width, v.uv[1] * inv_height};
			if (wedge_idx >= mesh.wedges.size()) {
				new_tri.w[k] = wedge_idx;
				continue;
			}
			const Wedge& old_w = mesh.wedges[wedge_idx];
			int32_t qu = static_cast<int32_t>(std::lround(uv.u * quant));
			int32_t qv = static_cast<int32_t>(std::lround(uv.v * quant));
			WedgeKey key{old_w.p, old_w.n, qu, qv};
			auto it = wedge_map.find(key);
			if (it == wedge_map.end()) {
				Index tex_idx = static_cast<Index>(texcoords.size());
				texcoords.push_back(uv);
				Wedge w;
				w.p = old_w.p;
				w.n = old_w.n;
				w.t = tex_idx;
				Index new_idx = static_cast<Index>(new_wedges.size());
				new_wedges.push_back(w);
				wedge_map.emplace(key, new_idx);
				new_tri.w[k] = new_idx;
			} else {
				new_tri.w[k] = it->second;
			}
		}
		new_triangles[i] = new_tri;
	}

	mesh.wedges = std::move(new_wedges);
	mesh.triangles = std::move(new_triangles);
	if (out_texcoords) {
		*out_texcoords = std::move(texcoords);
	}
	xatlas::Destroy(atlas);
	return true;
}

TileMap rasterize(std::vector<Material> &materials, MergedMesh &source, MergedMesh &destination, int tex_res) {

}


} // namespace nx
