#include "parametrization.h"

#include "xatlas.h"
#include "texture_cache.h"
#include "../core/mappedmesh.h"
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

bool create_parametrization(NodeMesh& mesh, const ParametrizationOptions& options, std::vector<Vector2f>* out_texcoords) {
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

TileMap rasterize(std::vector<Material> &materials, NodeMesh &source, NodeMesh &destination, int tex_res) {

}

//This is done after loading the initial mesh IF mesh has textures
//we need to compute a new parametrization for each microcluster, and reproject the texture.
//for each micronode, we collect the clusters
//create a NodeMesh, reparametrize, create new wedges and textures. (those are local per microcluster).
//replace old wedges and textures,  positions and normals are not touched, triangles only updated in wedge index.
//reprojection is easy because the geometry is not touched, so rasterization in texture space is enough
//Having multiple threads is good but we might want to ensure the same order.

void reparametrize_clusters(MappedMesh& mesh) {

	ParametrizationOptions options;
	TextureCache texture_cache(mesh.dir.string());
	texture_cache.initFromMaterials(mesh.materials);
	//TODO constant size is not good enough, but for now...
	const int tex_res = options.resolution > 0 ? static_cast<int>(options.resolution) : 256;
	const int components = 3;

	// Allocate texels for each micronode.
	mesh.node_textures.resize(mesh.micronodes.size());
	std::size_t total_texels = 0;
	for (std::size_t i = 0; i < mesh.micronodes.size(); ++i) {
		NodeTexture& node_texture = mesh.node_textures[i];
		node_texture.offset = total_texels;
		node_texture.width = tex_res;
		node_texture.height = tex_res;
		node_texture.components = components;
		node_texture.tile_size = tex_res;
		node_texture.mip_count = 1;
		total_texels += node_texture_bytes(node_texture);
	}
	if (mesh.texels.size() == 0) {
		std::filesystem::path texel_path = mesh.dir / "texels.bin";
		if (!mesh.texels.open(texel_path.string(), MappedFile::READ_WRITE, total_texels)) {
			throw std::runtime_error("Could not create texels file: " + texel_path.string());
		}
	} else if (mesh.texels.size() != total_texels) {
		if (!mesh.texels.resize(total_texels)) {
			throw std::runtime_error("Could not resize texels file");
		}
	}

	// Create a temporary mapped array to store new wedges (one per triangle corner).
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<uint32_t> dis(0, 0xFFFFFFFFu);
	std::stringstream ss;
	ss << "nxs_tmp_wedges_" << std::hex << std::setfill('0') << std::setw(8) << dis(gen);
	std::filesystem::path tmp_path = std::filesystem::temp_directory_path() / (ss.str() + ".bin");


	MappedArray<Wedge> temp_wedges;
	std::size_t max_wedges = static_cast<std::size_t>(mesh.triangles.size()) * 3;
	if (!temp_wedges.open(tmp_path.string(), MappedFile::READ_WRITE, max_wedges)) {
		throw std::runtime_error("Could not create temporary wedge file: " + tmp_path.string());
	}
	std::atomic<Index> next_wedge{0};

	std::mutex mesh_lock;
	dp::thread_pool pool;

	mesh.node_textures.resize(mesh.micronodes.size());
	for (Index micro_id = 0; micro_id < mesh.micronodes.size(); micro_id++) {
		pool.enqueue_detach([&mesh, &mesh_lock, &temp_wedges, &next_wedge, options, tex_res, components](Index micro_id) {
			MicroNode& micronode = mesh.micronodes[micro_id];
			NodeTexture &node_texture = mesh.node_textures[micro_id];
			NodeMesh merged = merge_micronode_clusters(mesh, micronode);
			NodeMesh original = merged;

			// Keep a local copy of previous triangles and wedges (for reprojection).
			std::vector<Triangle> old_triangles = merged.triangles;
			std::vector<Wedge> old_wedges = merged.wedges;

			// Reparametrize merged mesh in-place.
			create_parametrization(merged, options);

			auto &materials = mesh.materials;
			//TODO rasterize in texture coords the mesh, use the original texture coords (and material_id) to sample the original texel.
			//create a function for rasterization in parametrization.h/cpp

			rasterize(materials, original, merged, tex_res);

			{
				std::lock_guard<std::mutex> lock(mesh_lock);

				// Write texels to mesh.texels for this micronode.
				std::size_t offset = mesh.node_textures[micro_id].offset;
				assert(merged.texels.size() <= node_texture_bytes(node_texture));
				uint8_t* dst = mesh.texels.data() + offset;
				std::memcpy(dst, merged.texels.data(), merged.texels.size());
				node_texture.offset = offset;
				node_texture.width = merged.width;
				node_texture.height = merged.height;
				node_texture.components = components;
				node_texture.tile_size = merged.width;
				node_texture.mip_count = 1;

				size_t count = 0;
				for (Index cluster_id : micronode.cluster_ids) {
					const Cluster& cluster = mesh.clusters[cluster_id];
					Index start = cluster.triangle_offset;
					Index end = cluster.triangle_offset + cluster.triangle_count;

					for(Index i = start; i < end; i++) {
						Index new_w[3];
						for (int k = 0; k < 3; ++k) {
							Index merged_w = merged.triangles[count].w[k];
							if (merged_w >= merged.wedges.size()) {
								new_w[k] = 0;
								continue;
							}
							Wedge out = merged.wedges[merged_w];
							if (out.p < merged.position_remap.size()) {
								out.p = merged.position_remap[out.p];
							}
							Index idx = next_wedge.fetch_add(1);
							if (idx < temp_wedges.size()) {
								temp_wedges[idx] = out;
							}
							new_w[k] = idx;
						}
						Triangle &tri = mesh.triangles[i];
						tri.w[0] = new_w[0];
						tri.w[1] = new_w[1];
						tri.w[2] = new_w[2];
						count++;
					}
				}
			}
		}, micro_id);
	}
	pool.wait_for_tasks();

	Index final_wedges = next_wedge.load();
	if (final_wedges < temp_wedges.size()) {
		temp_wedges.resize(final_wedges);
	}
	mesh.wedges.close();
	mesh.wedges = std::move(temp_wedges);
}




} // namespace nx
