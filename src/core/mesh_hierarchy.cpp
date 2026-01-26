#include "mesh_hierarchy.h"
#include "../nxsbuild/build_parameters.h"
#include "../nxsbuild/merge_simplify_split.h"
#include "clustering.h"
#include "clustering_metis.h"
#include "micro_clustering_metis.h"
#include "adjacency.h"
#include "spatial_sort.h"
#include "../loaders/plyexporter.h"

#include <iostream>
#include <algorithm>
#include <filesystem>

namespace nx {



void copyVertices(MeshFiles &source, MeshFiles &target) {
	target.positions.resize(source.positions.size());
	std::copy(source.positions.data(), source.positions.data() + source.positions.size(),
			  target.positions.data());

	target.wedges.resize(source.wedges.size());
	std::copy(source.wedges.data(), source.wedges.data() + source.wedges.size(),
			  target.wedges.data());

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

	std::vector<Index> wedge_remap(wedge_count, std::numeric_limits<Index>::max());
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

	// Compact positions based on wedge usage
	std::size_t pos_count = mesh.positions.size();
	std::vector<uint8_t> pos_used(pos_count, 0);
	for(int i = 0; i < mesh.wedges.size(); i++) {
		const Wedge &w = mesh.wedges[i];
		if (w.p < pos_count) {
			pos_used[w.p] = 1;
		}
	}

	std::vector<Index> pos_remap(pos_count, std::numeric_limits<Index>::max());
	Index new_pos_count = 0;
	for (Index i = 0; i < pos_count; ++i) {
		if (pos_used[i]) {
			pos_remap[i] = new_pos_count;
			if (new_pos_count != i) {
				mesh.positions[new_pos_count] = mesh.positions[i];
				if (mesh.colors.size() > 0) {
					mesh.colors[new_pos_count] = mesh.colors[i];
				}
				if (mesh.material_ids.size() > 0) {
					mesh.material_ids[new_pos_count] = mesh.material_ids[i];
				}
			}
			new_pos_count++;
		}
	}
	mesh.positions.resize(new_pos_count);
	if (mesh.colors.size() > 0) {
		mesh.colors.resize(new_pos_count);
	}
	if (mesh.material_ids.size() > 0) {
		mesh.material_ids.resize(new_pos_count);
	}

	for(int i = 0; i < mesh.wedges.size(); i++) {
		Wedge &w = mesh.wedges[i];
		w.p = pos_remap[w.p];
	}
}
void MeshHierarchy::initialize(MeshFiles&& base_mesh) {
	nx::spatial_sort_mesh(base_mesh);
	nx::compute_adjacency(base_mesh);

	levels.push_back(std::move(base_mesh));
}

void MeshHierarchy::build_hierarchy(const BuildParameters& params) {
	if (levels.empty()) {
		throw std::runtime_error("MeshHierarchy not initialized. Call initialize() first.");
	}

	// Build initial clusters and micronodes
	std::cout << "Building initial clusters and micronodes..." << std::endl;
	std::size_t max_triangles = params.faces_per_cluster;
	MeshFiles &mesh = levels[0];

	nx::build_clusters(mesh, max_triangles,
					   params.use_metis ? nx::ClusteringMethod::Metis : nx::ClusteringMethod::Greedy);

	mesh.micronodes = nx::create_micronodes_metis(mesh, params.clusters_per_node);

	std::size_t level_index = 0;
	
	while (true) {
		MeshFiles &current = levels.back();
		std::cout << "\n--- Level " << level_index << " ---" << std::endl;
		std::cout << "Micronodes: " << current.micronodes.size() << std::endl;
		std::cout << "Triangles: " << current.triangles.size() << std::endl;

		if(current.micronodes.size() == 1)
			break;
		
		// Create next level mesh and process
		MeshFiles next_level;
		process_level(current, next_level, params.clusters_per_node);
		
		// Add the new level
		levels.push_back(std::move(next_level));

		level_index++;
	}
	
	std::cout << "\n=== Hierarchical Simplification Complete ===" << std::endl;
}

void MeshHierarchy::process_level(MeshFiles& mesh, MeshFiles& next_mesh, std::size_t clusters_per_node) {
	// Copy geometry (positions/wedges/colors/material_ids)
	copyVertices(mesh, next_mesh);


	// This creates micronodes in next_mesh with children references and centroids
	create_parent_micronodes(mesh, next_mesh, clusters_per_node);

	// Now iterate on the current mesh micronodes to merge, simplify, and split
	for (const MicroNode& micronode : mesh.micronodes) {
		// 1) Collect its clusters into a temporary mesh and lock boundaries.
		MergedMesh merged = merge_micronode_clusters(mesh, micronode);

		// 2) Simplify node triangles (vertex collapse moves positions in new mesh)
		const Index target_triangle_count = std::max<Index>(1, merged.triangles.size() / 2);
		simplify_mesh_clustered(merged, target_triangle_count);

		// TODO: update vertices and wedges in next_mesh according to merged.position_map
		update_vertices_and_wedges(next_mesh, merged);

		// TODO: split according to primary and secondary partitions.
		// TODO: create 2 clusters from the simplified mesh
		// TODO: add the cluster_id to the corresponding micronodes (via children_nodes)
		split_mesh(merged, micronode, next_mesh);
	}

	// TODO: Implement compaction (remove unused vertices/wedges and remap indices)
	compact_mesh(next_mesh);

	// Recompute adjacency for the next level
	if (next_mesh.triangles.size() > 0) {
		compute_adjacency(next_mesh);
	}
	// export to the ply by cluster and by node
	{
		const std::filesystem::path out_dir = std::filesystem::current_path();
		const std::string level_tag = "level_" + std::to_string(next_mesh.micronodes.size());
		export_ply(next_mesh, out_dir / (level_tag + "_clusters.ply"), ColoringMode::ByCluster);
		export_ply(next_mesh, out_dir / (level_tag + "_nodes.ply"), ColoringMode::ByMicroNode);
	}

}





} // namespace nx
