#include "mappedmesh.h"

#include <iostream>
#include <filesystem>
#include <sstream>


namespace nx {

MappedMesh::MappedMesh() {
	create(std::filesystem::path(MappedFile::makeTempPath(std::filesystem::current_path().string(), "nxs_cache_")));
}

MappedMesh::~MappedMesh() {
	close();
}

bool MappedMesh::create(const std::filesystem::path& dir_path) {
	close();
	std::filesystem::create_directories(dir_path);
	dir = dir_path;
	bounds = Aabb{{0,0,0},{0,0,0}};

	// Start empty; callers resize once counts are known.
	if (!mapDataFiles(MappedFile::READ_WRITE)) {
		close();
		return false;
	}
	return true;
}

void MappedMesh::close() {
	positions.close();
	colors.close();
	normals.close();
	texcoords.close();
	wedges.close();
	triangles.close();
	material_ids.close();
	adjacency.close();
	clusters.close();
	triangle_to_cluster.close();
	node_textures.close();
	texels.close();

	if(!dir.empty())
		std::filesystem::remove_all(dir);
	dir.clear();
	bounds = Aabb{{0,0,0},{0,0,0}};
}

bool MappedMesh::mapDataFiles(MappedFile::Mode mode) {
	if (!positions.open(pathFor("positions.bin").string(), mode, 0)) return false;
	if (!colors.open(pathFor("colors.bin").string(), mode, 0)) return false;
	if (!normals.open(pathFor("normals.bin").string(), mode, 0)) return false;
	if (!texcoords.open(pathFor("textures.bin").string(), mode, 0)) return false;

	if (!wedges.open(pathFor("wedges.bin").string(), mode, 0)) return false;
	if (!triangles.open(pathFor("triangles.bin").string(), mode, 0)) return false;
	if (!material_ids.open(pathFor("material_ids.bin").string(), mode, 0)) return false;
	if (!adjacency.open(pathFor("adjacency.bin").string(), mode, 0)) return false;
	if (!clusters.open(pathFor("clusters.bin").string(), mode, 0)) return false;
	if (!triangle_to_cluster.open(pathFor("triangle_to_cluster.bin").string(), mode, 0)) return false;
	if (!node_textures.open(pathFor("node_textures.bin").string(), mode, 0)) return false;
	if (!texels.open(pathFor("texels.bin").string(), mode, 0)) return false;
	return true;
}

// Allocate node textures and texels for micronodes
void MappedMesh::allocate_node_textures_and_texels(int tex_res, int components) {
	node_textures.resize(micronodes.size());
	std::size_t total_texels = 0;
	for(std::size_t i = 0; i < micronodes.size(); ++i) {
		NodeTexture& node_texture = node_textures[i];
		node_texture.offset = total_texels;
		node_texture.width = tex_res;
		node_texture.height = tex_res;
		node_texture.components = components;
		node_texture.mip_count = 1;
		total_texels += node_texture_bytes(node_texture);
	}

	if(texels.size() != total_texels) {
		if(!texels.resize(total_texels)) {
			throw std::runtime_error("Could not resize texels file");
		}
	}
}

// Save micronodes and macronodes to a JSON file for state persistence
void MappedMesh::saveState(const std::filesystem::path& filepath) {
	using json = nlohmann::json;
	json j;

	// Serialize micronodes
	j["micronodes"] = json::array();
	for (const auto& node : micronodes) {
		json node_j;
		node_j["id"] = node.id;
		node_j["cluster_ids"] = node.cluster_ids;
		node_j["triangle_count"] = node.triangle_count;
		node_j["vertex_count"] = node.vertex_count;
		node_j["centroid"] = {node.centroid.x, node.centroid.y, node.centroid.z};
		node_j["center"] = {node.center.x, node.center.y, node.center.z};
		node_j["radius"] = node.radius;
		node_j["error"] = node.error;
		j["micronodes"].push_back(node_j);
	}

	// Serialize macronodes
	j["macronodes"] = json::array();
	for (const auto& node : macronodes) {
		json node_j;
		node_j["id"] = node.id;
		node_j["micronode_ids"] = node.micronode_ids;
		node_j["triangle_count"] = node.triangle_count;
		node_j["centroid"] = {node.centroid.x, node.centroid.y, node.centroid.z};
		node_j["center"] = {node.center.x, node.center.y, node.center.z};
		node_j["radius"] = node.radius;
		j["macronodes"].push_back(node_j);
	}

	// Write to file and ensure data is flushed to disk.
	std::ostringstream ss;
	ss << std::setw(4) << j << std::endl;
	const std::string out_str = ss.str();

	// Ensure all mapped files are flushed to disk so we can recover after a crash
	positions.sync();
	colors.sync();
	normals.sync();
	texcoords.sync();
	wedges.sync();
	triangles.sync();
	material_ids.sync();
	adjacency.sync();
	clusters.sync();
	triangle_to_cluster.sync();
	node_textures.sync();
	texels.sync();
}

// Load micronodes and macronodes from a JSON file
void MappedMesh::loadState(const std::filesystem::path& filepath) {
	using json = nlohmann::json;
	std::ifstream i(filepath.string());
	if (!i.is_open()) {
		throw std::runtime_error("Could not open state file for loading: " + filepath.string());
	}
	json j;
	i >> j;

	// Clear existing state
	micronodes.clear();
	macronodes.clear();

	// Load micronodes
	if (j.contains("micronodes") && j["micronodes"].is_array()) {
		for (const auto& node_j : j["micronodes"]) {
			MicroNode node;
			node.id = node_j["id"].get<Index>();
			node.cluster_ids = node_j["cluster_ids"].get<std::vector<Index>>();
			node.triangle_count = node_j["triangle_count"].get<Index>();
			node.vertex_count = node_j["vertex_count"].get<Index>();
			node.centroid = {node_j["centroid"][0].get<float>(), node_j["centroid"][1].get<float>(), node_j["centroid"][2].get<float>()};
			node.center = {node_j["center"][0].get<float>(), node_j["center"][1].get<float>(), node_j["center"][2].get<float>()};
			node.radius = node_j["radius"].get<float>();
			node.error = node_j["error"].get<float>();
			micronodes.push_back(node);
		}
	}

	// Load macronodes
	if (j.contains("macronodes") && j["macronodes"].is_array()) {
		for (const auto& node_j : j["macronodes"]) {
			MacroNode node;
			node.id = node_j["id"].get<Index>();
			node.micronode_ids = node_j["micronode_ids"].get<std::vector<Index>>();
			node.triangle_count = node_j["triangle_count"].get<Index>();
			node.centroid = {node_j["centroid"][0].get<float>(), node_j["centroid"][1].get<float>(), node_j["centroid"][2].get<float>()};
			node.center = {node_j["center"][0].get<float>(), node_j["center"][1].get<float>(), node_j["center"][2].get<float>()};
			node.radius = node_j["radius"].get<float>();
			macronodes.push_back(node);
		}
	}
	mapDataFiles(MappedFile::READ_WRITE);
	has_colors = colors.size() > 0;
	has_normals = normals.size() > 0;
	has_textures = texcoords.size() > 0;

}

}
