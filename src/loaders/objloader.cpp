#include "objloader.h"
#include "meshloader.h"
#include "mappedmesh.h"
#include <filesystem>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <fstream>
#include <cerrno>
#include <cstring>

namespace fs = std::filesystem;

namespace nx {


ObjLoader::ObjLoader(const std::string& filename, const std::string& mtl_path)
	: obj_path(filename) {

	if (!mtl_path.empty()) {
		use_custom_mtl = true;
		mtl_files.push_back(mtl_path);
	}

	read_mtls();
}

void ObjLoader::read_mtls() {
	// First pass: find mtllib declarations if no custom MTL specified
	if (!use_custom_mtl) {
		std::ifstream file(obj_path);
		if (!file.is_open()) {
			std::string reason = (errno ? std::strerror(errno) : "unknown");
			throw std::runtime_error("Could not open OBJ file: " + obj_path + " (" + reason + ")");
		}

		std::string line;
		while (std::getline(file, line)) {
			std::istringstream iss(line);
			std::string cmd;
			iss >> cmd;

			if (cmd == "mtllib") {
				std::string mtl_file;
				std::getline(iss >> std::ws, mtl_file);
				// Strip trailing whitespace (including \r from Windows line endings)
				mtl_file.erase(mtl_file.find_last_not_of(" \t\r\n") + 1);
				mtl_file = resolveTexturePath(obj_path, mtl_file);
				mtl_files.push_back(mtl_file);
			}
		}

		// If no mtllib found, try same name as OBJ
		if (mtl_files.empty()) {
			fs::path obj_file(obj_path);
			fs::path mtl_file = obj_file.parent_path() / (obj_file.stem().string() + ".mtl");
			if (fs::exists(mtl_file)) {
				mtl_files.push_back(mtl_file.string());
			}
		}
	}

	// Read all MTL files
	for (const auto& mtl_file : mtl_files) {
		read_mtl(mtl_file);
	}
}

void ObjLoader::read_mtl(const std::string& mtl_path) {
	if (!fs::exists(mtl_path)) {
		std::cerr << "MTL file not found: " << mtl_path << std::endl;
		return;
	}

	std::ifstream mtl_file(mtl_path);
	if (!mtl_file.is_open()) {
		std::cerr << "Could not open MTL file: " << mtl_path << std::endl;
		return;
	}

	Material current_material;
	std::string current_name;
	bool has_material = false;

	std::string line;
	while (std::getline(mtl_file, line)) {
		std::istringstream iss(line);
		std::string cmd;
		iss >> cmd;

		if (cmd.empty() || cmd[0] == '#') continue;

		if (cmd == "newmtl") {
			if (has_material && !current_name.empty()) {
				material_map[current_name] = static_cast<int32_t>(materials.size());
				materials.push_back(current_material);
			}

			iss >> current_name;
			current_material = Material();
			current_material.type = MaterialType::PBR;
			has_material = true;
		}
		else if (cmd == "Kd") {
			float r, g, b;
			iss >> r >> g >> b;
			current_material.base_color[0] = r;
			current_material.base_color[1] = g;
			current_material.base_color[2] = b;
		}
		else if (cmd == "d" || cmd == "Tr") {
			float alpha;
			iss >> alpha;
			if (cmd == "Tr") alpha = 1.0f - alpha;
			current_material.base_color[3] = alpha;
		}
		else if (cmd == "map_Kd" || cmd == "map_Ka") {
			std::string tex_path;
			std::getline(iss >> std::ws, tex_path);
			current_material.base_color_texture = resolveTexturePath(mtl_path, tex_path);
		}
		else if (cmd == "map_Ks") {
			std::string tex_path;
			std::getline(iss >> std::ws, tex_path);
			current_material.specular_texture = resolveTexturePath(mtl_path, tex_path);
		}
		else if (cmd == "map_Bump" || cmd == "bump") {
			std::string tex_path;
			std::getline(iss >> std::ws, tex_path);
			current_material.normal_texture = resolveTexturePath(mtl_path, tex_path);
		}
		else if (cmd == "Ks") {
			float r, g, b;
			iss >> r >> g >> b;
			current_material.specular[0] = r;
			current_material.specular[1] = g;
			current_material.specular[2] = b;
			current_material.type = MaterialType::PHONG;
		}
		else if (cmd == "Ns") {
			float shininess;
			iss >> shininess;
			current_material.shininess = shininess;
			current_material.type = MaterialType::PHONG;
		}
		else if (cmd == "Pr") {
			float roughness;
			iss >> roughness;
			current_material.roughness_factor = roughness;
		}
		else if (cmd == "Pm") {
			float metallic;
			iss >> metallic;
			current_material.metallic_factor = metallic;
		}
	}

	if (has_material && !current_name.empty()) {
		material_map[current_name] = static_cast<int32_t>(materials.size());
		materials.push_back(current_material);
	}
}

void ObjLoader::load(MappedMesh& mesh) {
	std::ifstream file(obj_path);
	if (!file.is_open()) {
		std::string reason = (errno ? std::strerror(errno) : "unknown");
		throw std::runtime_error("Could not open OBJ file: " + obj_path + " (" + reason + ")");
	}

	// First pass: count and collect all vertex data
	std::string line;
	Index position_count = 0;
	Index normal_count = 0;
	Index texcoord_count = 0;
	Index triangle_count = 0;

	while (std::getline(file, line)) {
		if (line.empty()) continue;
		char first = line[0];

		if (first == 'v') {
			if (line[1] == ' ') position_count++;
			else if (line[1] == 'n') normal_count++;
			else if (line[1] == 't') texcoord_count++;
		}
		else if (first == 'f') {
			// Count triangles (fan triangulation)
			std::istringstream iss(line);
			std::string cmd, tmp;
			iss >> cmd;
			int vertex_count = 0;
			while (iss >> tmp) vertex_count++;
			if (vertex_count >= 3) {
				triangle_count += (vertex_count - 2);
			}
		}
	}

	// allocate storage
	mesh.positions.resize(position_count);
	mesh.normals.resize(normal_count);
	mesh.texcoords.resize(texcoord_count);

	// Second pass: load all vertex attributes
	file.clear();
	file.seekg(0);

	Aabb bounds;
	bool first_vertex = true;

	Index current_p = 0;
	Index current_n = 0;
	Index current_t = 0;

	while (std::getline(file, line)) {
		if (line.empty()) continue;
		std::istringstream iss(line);
		std::string cmd;
		iss >> cmd;

		if (cmd == "v") {
			Vector3f p;
			iss >> p.x >> p.y >> p.z;

			if (first_vertex) {
				bounds.min = bounds.max = p;
				first_vertex = false;
			} else {
				bounds.min.x = std::min(bounds.min.x, p.x);
				bounds.min.y = std::min(bounds.min.y, p.y);
				bounds.min.z = std::min(bounds.min.z, p.z);
				bounds.max.x = std::max(bounds.max.x, p.x);
				bounds.max.y = std::max(bounds.max.y, p.y);
				bounds.max.z = std::max(bounds.max.z, p.z);
			}

			mesh.positions[current_p++] = p;
		}
		else if (cmd == "vn") {
			Vector3f n;
			iss >> n.x >> n.y >> n.z;
			mesh.normals[current_n++] = n;
		}
		else if (cmd == "vt") {
			Vector2f t;
			iss >> t.u >> t.v;
			mesh.texcoords[current_t++] = t;
		}
	}


	bool has_materials = !materials.empty();
	if (has_materials) {
		mesh.material_ids.resize(triangle_count);
		mesh.materials = materials;
	}

	// Third pass: build wedges and triangles
	file.clear();
	file.seekg(0);

	// Resize mesh arrays
	Index wedge_count = triangle_count * 3;
	mesh.wedges.resize(wedge_count);
	mesh.triangles.resize(triangle_count);

	Index current_wedge = 0;
	Index current_triangle = 0;
	current_material_id = -1;

	while (std::getline(file, line)) {
		if (line.empty()) continue;
		std::istringstream iss(line);
		std::string cmd;
		iss >> cmd;

		if (cmd == "usemtl") {
			std::string material_name;
			iss >> material_name;
			auto it = material_map.find(material_name);
			if (it != material_map.end()) {
				current_material_id = it->second;
			} else {
				current_material_id = -1;
			}
		}
		else if (cmd == "f") {
			std::vector<std::array<int, 3>> face_indices;
			std::string vertex_str;

			while (iss >> vertex_str) {
				if (vertex_str[0] == '#') break;

				std::array<int, 3> indices = {0, 0, 0};
				size_t pos = 0;
				int idx = 0;

				for (size_t i = 0; i <= vertex_str.length() && idx < 3; ++i) {
					if (i == vertex_str.length() || vertex_str[i] == '/') {
						if (i > pos) {
							indices[idx] = std::stoi(vertex_str.substr(pos, i - pos));
						}
						idx++;
						pos = i + 1;
					}
				}

				face_indices.push_back(indices);
			}

			// Fan triangulation
			int valence = static_cast<int>(face_indices.size());
			for (int i = 1; i < valence - 1; ++i) {
				// Triangle [0, i, i+1]
				for (int j = 0; j < 3; ++j) {
					int corner = (j == 0) ? 0 : i + j - 1;
					auto& idx = face_indices[corner];

					Wedge& wedge = mesh.wedges[current_wedge];

					// Position (1-based in OBJ, convert to 0-based)
					Index pos_idx = (idx[0] > 0) ? idx[0] - 1 : position_count + idx[0];
					if(pos_idx >= position_count)
						throw std::runtime_error("Position index larger then the number of v in obj");
					wedge.p = pos_idx;

					// Texcoord
					if (idx[1] != 0) {
						Index tc_idx = (idx[1] > 0) ? idx[1] - 1 : texcoord_count + idx[1];
						if(tc_idx >= texcoord_count)
							throw std::runtime_error("Texture index larger then the number of vt in obj");
						wedge.t = tc_idx;
					}

					// Normal
					if (idx[2] != 0) {
						Index n_idx = (idx[2] > 0) ? idx[2] - 1 : normal_count + idx[2];
						if(n_idx >= normal_count)
							throw std::runtime_error("Normal index larger then the number of vn in obj");
						wedge.n = n_idx;
					}

					mesh.triangles[current_triangle].w[j] = current_wedge;
					current_wedge++;
				}

				if (has_materials) {
					mesh.material_ids[current_triangle] = current_material_id;
				}

				current_triangle++;
			}
		}
	}

	mesh.bounds = bounds;
}

} // namespace nx
