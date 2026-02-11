#include "mesh.h"
#include <random>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <filesystem>
#include <random>

namespace nx {

MeshFiles::MeshFiles() {
	// Create temporary directory with random suffix
	namespace fs = std::filesystem;
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<uint32_t> dis(0, 0xFFFFFFFF);
	std::stringstream ss;
	ss << "nxs_cache_" << std::hex << std::setfill('0') << std::setw(8) << dis(gen);

	fs::path tmp = fs::current_path() / ss.str();
	create(tmp);
}

MeshFiles::~MeshFiles() {
	close();
}

bool MeshFiles::create(const std::filesystem::path& dir_path) {
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

void MeshFiles::close() {
	positions.close();
	colors.close();
	normals.close();
	texcoords.close();
	wedges.close();
	triangles.close();
	material_ids.close();
	adjacency.close();
	clusters.close();

	if(!dir.empty())
		std::filesystem::remove_all(dir);
	dir.clear();
	bounds = Aabb{{0,0,0},{0,0,0}};
}

bool MeshFiles::mapDataFiles(MappedFile::Mode mode) {
	if (!positions.open(pathFor("positions.bin").string(), mode, 0)) return false;
	if (!colors.open(pathFor("colors.bin").string(), mode, 0)) return false;
	if (!normals.open(pathFor("normals.bin").string(), mode, 0)) return false;
	if (!texcoords.open(pathFor("textures.bin").string(), mode, 0)) return false;

	if (!wedges.open(pathFor("wedges.bin").string(), mode, 0)) return false;
	if (!triangles.open(pathFor("triangles.bin").string(), mode, 0)) return false;
	if (!material_ids.open(pathFor("material_ids.bin").string(), mode, 0)) return false;
	if (!adjacency.open(pathFor("adjacency.bin").string(), mode, 0)) return false;
	if (!clusters.open(pathFor("clusters.bin").string(), mode, 0)) return false;
	return true;
}

} // namespace nx
