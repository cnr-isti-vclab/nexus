#include "mesh_hierarchy.h"
#include "../nxsbuild/build_parameters.h"
#include "spatial_sort.h"
#include "adjacency.h"
#include "clustering.h"
#include "micro_clustering.h"
#include "../nxsbuild/merge_simplify_split.h"

#include "../loaders/plyexporter.h"
#include "../core/thread_pool.h"
#include "../texture/parametrization.h"
#include "../loaders/objexporter.h"
#include <iostream>
#include <algorithm>
#include <filesystem>
#include <set>
#include <mutex>
#include <cmath>
#include <atomic>
#include <random>
#include <sstream>
#include <iomanip>
#include <cstring>
#include "../core/log.h"

namespace nx {



void copyVertices(MeshFiles &source, MeshFiles &target) {
	target.positions.resize(source.positions.size());
	std::copy(source.positions.data(), source.positions.data() + source.positions.size(),
			  target.positions.data());
	if (source.colors.size() > 0) {
		target.colors.resize(source.colors.size());
		std::copy(source.colors.data(), source.colors.data() + source.colors.size(),
				  target.colors.data());
	}

	if (source.material_ids.size() > 0) {
		target.material_ids.resize(source.material_ids.size());
		std::copy(source.material_ids.data(), source.material_ids.data() + source.material_ids.size(),
				  target.material_ids.data());
	}
}

void compactPositions(MeshFiles& mesh) {
	// Compact positions based on wedge usage
	std::size_t pos_count = mesh.positions.size();
	std::vector<uint8_t> pos_used(pos_count, 0);
	for(int i = 0; i < mesh.wedges.size(); i++) {
		const Wedge &w = mesh.wedges[i];
		pos_used[w.p] = 1;
	}

	std::vector<Index> pos_remap(pos_count, NONE);
	Index new_pos_count = 0;
	for (Index i = 0; i < pos_count; ++i) {
		if (pos_used[i]) {
			pos_remap[i] = new_pos_count;
			if (new_pos_count != i) {
				mesh.positions[new_pos_count] = mesh.positions[i];
				if (mesh.has_colors) {
					mesh.colors[new_pos_count] = mesh.colors[i];
				}
			}
			new_pos_count++;
		}
	}
	mesh.positions.resize(new_pos_count);
	if (mesh.has_colors) {
		mesh.colors.resize(new_pos_count);
	}

	for(int i = 0; i < mesh.wedges.size(); i++) {
		Wedge &w = mesh.wedges[i];
		w.p = pos_remap[w.p];
	}
}

void compactNormals(MeshFiles& mesh) {
	if(!mesh.has_normals)
		return;
	std::size_t n_count = mesh.normals.size();
	std::vector<uint8_t> pos_used(n_count, 0);
	for(int i = 0; i < mesh.wedges.size(); i++) {
		const Wedge &w = mesh.wedges[i];
		pos_used[w.n] = 1;
	}

	std::vector<Index> pos_remap(n_count, NONE);
	Index new_pos_count = 0;
	for (Index i = 0; i < n_count; ++i) {
		if (pos_used[i]) {
			pos_remap[i] = new_pos_count;
			if (new_pos_count != i) {
				mesh.normals[new_pos_count] = mesh.normals[i];
			}
			new_pos_count++;
		}
	}
	mesh.normals.resize(new_pos_count);
	for(int i = 0; i < mesh.wedges.size(); i++) {
		Wedge &w = mesh.wedges[i];
		w.n = pos_remap[w.n];
	}
}

void compactTextures(MeshFiles& mesh) {
	if(!mesh.has_textures)
		return;

	std::size_t tex_count = mesh.texcoords.size();
	std::vector<uint8_t> pos_used(tex_count, 0);
	for(int i = 0; i < mesh.wedges.size(); i++) {
		const Wedge &w = mesh.wedges[i];
		pos_used[w.t] = 1;
	}

	std::vector<Index> tex_remap(tex_count, NONE);
	Index new_tex_count = 0;
	for (Index i = 0; i < tex_count; ++i) {
		if (pos_used[i]) {
			tex_remap[i] = new_tex_count;
			if (new_tex_count != i) {
				mesh.texcoords[new_tex_count] = mesh.texcoords[i];
			}
			new_tex_count++;
		}
	}
	mesh.texcoords.resize(new_tex_count);

	for(int i = 0; i < mesh.wedges.size(); i++) {
		Wedge &w = mesh.wedges[i];
		w.t = tex_remap[w.t];
	}
}

void compact_mesh(MeshFiles& mesh) {
	// Compact wedges based on triangle usage
	std::size_t wedge_count = mesh.wedges.size();
	std::vector<uint8_t> wedge_used(wedge_count, 0);
	for(size_t i = 0; i < mesh.triangles.size(); i++) {
		Triangle &tri = mesh.triangles[i];
		for (int k = 0; k < 3; ++k) {
			Index w = tri.w[k];
			if (w < wedge_count) {
				wedge_used[w] = 1;
			}
		}
	}

	std::vector<Index> wedge_remap(wedge_count, NONE);
	Index new_wedge_count = 0;
	for (Index i = 0; i < wedge_count; ++i) {
		if (wedge_used[i]) {
			wedge_remap[i] = new_wedge_count;
			if (new_wedge_count != i) {
				mesh.wedges[new_wedge_count] = mesh.wedges[i];
			}
			new_wedge_count++;
		}
	}
	mesh.wedges.resize(new_wedge_count);

	for(size_t i = 0; i < mesh.triangles.size(); i++) {
		Triangle &tri = mesh.triangles[i];
		for (int k = 0; k < 3; ++k) {
			Index old_w = tri.w[k];
			tri.w[k] = wedge_remap[old_w];
		}
	}

	compactPositions(mesh);
	compactNormals(mesh);
	compactTextures(mesh);
}



//TODO this should identify creases, compute normals and reunify wedges.
void recompute_normals(MeshFiles& mesh) {
	mesh.normals.resize(mesh.positions.size());
	for(size_t i = 0; i < mesh.normals.size(); i++)
		mesh.normals[i] = {0.0f, 0.0f, 0.0f};


	for (size_t i = 0; i < mesh.triangles.size(); ++i) {
		const Triangle &tri = mesh.triangles[i];
		Index w0 = tri.w[0];
		Index w1 = tri.w[1];
		Index w2 = tri.w[2];

		const Vector3f &p0 = mesh.positions[mesh.wedges[w0].p];
		const Vector3f &p1 = mesh.positions[mesh.wedges[w1].p];
		const Vector3f &p2 = mesh.positions[mesh.wedges[w2].p];

		float ux = p1.x - p0.x;
		float uy = p1.y - p0.y;
		float uz = p1.z - p0.z;
		float vx = p2.x - p0.x;
		float vy = p2.y - p0.y;
		float vz = p2.z - p0.z;

		Vector3f n;
		n.x = uy * vz - uz * vy;
		n.y = uz * vx - ux * vz;
		n.z = ux * vy - uy * vx;

		Index v0 = mesh.wedges[w0].p;
		Index v1 = mesh.wedges[w1].p;
		Index v2 = mesh.wedges[w2].p;

		Vector3f &n0 = mesh.normals[v0];
		Vector3f &n1 = mesh.normals[v1];
		Vector3f &n2 = mesh.normals[v2];

		n0.x += n.x; n0.y += n.y; n0.z += n.z;
		n1.x += n.x; n1.y += n.y; n1.z += n.z;
		n2.x += n.x; n2.y += n.y; n2.z += n.z;
	}

	for(size_t i = 0; i < mesh.normals.size(); i++) {
		Vector3f &n = mesh.normals[i];
		float len = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
		if (len > 0.0f) {
			n.x /= len;
			n.y /= len;
			n.z /= len;
		} else {
			n.x = n.y = n.z = 0.0f;
		}
	}
}

void MeshHierarchy::initialize(MeshFiles&& base_mesh) {
	nx::spatial_sort_mesh(base_mesh);
	nx::compute_adjacency(base_mesh);
	recompute_normals(base_mesh);

	levels.push_back(std::move(base_mesh));
}

void MeshHierarchy::build_hierarchy(const BuildParameters& params) {
	if (levels.empty()) {
		throw std::runtime_error("MeshHierarchy not initialized. Call initialize() first.");
	}

	// Build initial clusters and micronodes
	nx::log << "Building initial clusters and micronodes..." << std::endl;
	std::size_t max_triangles = params.faces_per_cluster;
	MeshFiles &mesh = levels[0];

	nx::build_clusters(mesh, max_triangles*params.clusters_per_node,
					   params.use_greedy ? nx::ClusteringMethod::Greedy : nx::ClusteringMethod::Metis);

	//split each cluster in 4 clusters and create a micronode for the 4.
	nx::split_clusters(mesh, max_triangles);

	// Reparametrize all clusters after initial split
	reparametrize_clusters(mesh, params);

	//parametrize and project texture
	if(1) {
		const std::filesystem::path out_dir = std::filesystem::current_path();
		const std::string level_tag = "level_0";
		export_ply(mesh, out_dir / (level_tag + "_clusters.ply"), ColoringMode::ByCluster);
		export_ply(mesh, out_dir / (level_tag + "_nodes.ply"), ColoringMode::ByMicroNode);
	}

	std::size_t level_index = 0;
	
	while (true) {
		MeshFiles &current = levels.back();
		nx::log << "\n--- Level " << level_index << " ---" << std::endl;
		nx::log << "Micronodes: " << current.micronodes.size() << std::endl;
		nx::log << "Triangles: " << current.triangles.size() << std::endl;

		if(current.micronodes.size() == 1)
			break;
		
		// Create next level mesh and process
		MeshFiles next_level;
		process_level(current, next_level, params);
		
		// Add the new level
		levels.push_back(std::move(next_level));

		level_index++;
	}
}

//This is done after loading the initial mesh IF mesh has textures
//we need to compute a new parametrization for each microcluster, and reproject the texture.
//for each micronode, we collect the clusters
//create a mergedmesh, reparametrize, create new wedges and textures. (those are local per microcluster).
//replace old wedges and textures,  positions and normals are not touched, triangles only updated in wedge index.
//reprojection is easy because the geometry is not touched, so rasterization in texture space is enough
//Having multiple threads is good but we might want to ensure the same order.

void MeshHierarchy::reparametrize_clusters(MeshFiles& mesh, const BuildParameters& params) {
	(void)params;
	ParametrizationOptions options;
	//TODO constant size is not good enough, but for now...
	const int tex_res = options.resolution > 0 ? static_cast<int>(options.resolution) : 256;
	const std::size_t texel_bytes = static_cast<std::size_t>(tex_res) * tex_res * 3;

	// Allocate texels for each micronode.
	mesh.node_textures.resize(mesh.micronodes.size());
	for (std::size_t i = 0; i < mesh.micronodes.size(); ++i) {
		mesh.node_textures[i].offset = i * texel_bytes;
		mesh.node_textures[i].width = tex_res;
		mesh.node_textures[i].height = tex_res;
	}
	std::size_t total_texels = mesh.node_textures.size() * texel_bytes;
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
		pool.enqueue_detach([&mesh, &mesh_lock, &temp_wedges, &next_wedge, options, tex_res, texel_bytes](Index micro_id) {
			MicroNode& micronode = mesh.micronodes[micro_id];
			NodeTexture &node_texture = mesh.node_textures[micro_id];
			MergedMesh merged = merge_micronode_clusters(mesh, micronode);
			MergedMesh original = merged;

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
				uint8_t* dst = mesh.texels.data() + offset;
				std::memcpy(dst, merged.texels.data(), merged.texels.size());
				node_texture.offset = offset;
				node_texture.width = merged.width;
				node_texture.height = merged.height;

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

void MeshHierarchy::process_level(MeshFiles& mesh, MeshFiles& next_mesh, const BuildParameters& params) {
	static int current_level = 1;
	// Copy geometry (positions/wedges/colors/material_ids)
	copyVertices(mesh, next_mesh);

	std::mutex next_mesh_lock;
	dp::thread_pool pool;

	// Now iterate on the current mesh micronodes to merge, simplify, and split
	for(Index micro_id = 0; micro_id < mesh.micronodes.size(); micro_id++) {


		// add tasks, in this case without caring about results of individual tasks
		pool.enqueue_detach([this, &mesh, &next_mesh, &params, &next_mesh_lock](Index micro_id) {
			MicroNode& micronode = mesh.micronodes[micro_id];


			// 1) Collect its clusters into a temporary mesh and lock boundaries.
			MergedMesh simplified = merge_micronode_clusters_for_simplification(mesh, micronode); //drop normals and textures.

			// 2) Simplify node triangles (vertex collapse moves positions in new mesh)
			const Index target_triangle_count = std::max<Index>(1, simplified.triangles.size() / 2);
			//simplify_mesh_edge(merged, target_triangle_count);
			micronode.error = simplify_mesh(simplified, target_triangle_count);

			if(mesh.has_textures) {
				//collect the original mesh for reprojection
				MergedMesh original = merge_micronode_clusters(mesh, micronode);

				if(1){
					MeshFiles debug_mesh;
					debug_mesh.positions.resize(original.positions.size());
					for (Index i = 0; i < original.positions.size(); ++i) {
						debug_mesh.positions[i] = original.positions[i];
					}
					debug_mesh.texcoords.resize(original.texcoords.size());
					for (Index i = 0; i < original.texcoords.size(); ++i) {
						debug_mesh.texcoords[i] = original.texcoords[i];
					}
					debug_mesh.wedges.resize(original.wedges.size());
					for (Index i = 0; i < original.wedges.size(); ++i) {
						debug_mesh.wedges[i] = original.wedges[i];
					}

					debug_mesh.triangles.resize(original.triangles.size());
					for (Index i = 0; i < original.triangles.size(); ++i) {
						debug_mesh.triangles[i] = original.triangles[i];
					}
					NodeTexture &notetex = mesh.node_textures[micro_id];

					export_obj(debug_mesh, "micronode.obj");
				}

				create_parametrization(simplified);
			}

			{
				const std::lock_guard<std::mutex> lock(next_mesh_lock);

				//tempoarily keep the dependencies in micronode.children_nodes
				split_mesh(simplified, micro_id, next_mesh, params.faces_per_cluster);
			}



			if(1){
				MeshFiles debug_mesh;
				debug_mesh.positions.resize(simplified.positions.size());
				for (Index i = 0; i < simplified.positions.size(); ++i) {
					debug_mesh.positions[i] = simplified.positions[i];
				}
				debug_mesh.wedges.resize(simplified.wedges.size());
				for (Index i = 0; i < simplified.wedges.size(); ++i) {
					debug_mesh.wedges[i] = simplified.wedges[i];
				}
				debug_mesh.triangles.resize(simplified.triangles.size());
				for (Index i = 0; i < simplified.triangles.size(); ++i) {
					debug_mesh.triangles[i] = simplified.triangles[i];
				}
				export_obj(debug_mesh, "debug_clustered.obj");
			}
		}, micro_id);
		pool.wait_for_tasks();

	}

	// TODO: Implement compaction (remove unused vertices/wedges and remap indices)
	compact_mesh(next_mesh);
	recompute_normals(next_mesh);

	// Recompute adjacency for the next level
	if (next_mesh.triangles.size() > 0) {
		compute_adjacency(next_mesh);
		
	}

	//TODO sort clusters  (including triangles) by center to improve locality?


// Returns a vector of MicroNode structures representing the partitions.
	next_mesh.micronodes = create_micronodes_metis(next_mesh, params.clusters_per_node, params.faces_per_cluster);

	// export to the ply by cluster and by node
	if(0){
		const std::filesystem::path out_dir = std::filesystem::current_path();
		const std::string level_tag = "level_" + std::to_string(current_level);
		export_ply(next_mesh, out_dir / (level_tag + "_clusters.ply"), ColoringMode::ByCluster);
		export_ply(next_mesh, out_dir / (level_tag + "_nodes.ply"), ColoringMode::ByMicroNode);
	}
	current_level++;
}





} // namespace nx
