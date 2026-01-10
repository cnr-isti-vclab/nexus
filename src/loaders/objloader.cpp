/*
Nexus

Copyright(C) 2012 - Federico Ponchio
ISTI - Italian National Research Council - Visual Computing Lab

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License (http://www.gnu.org/licenses/gpl.txt)
for more details.
*/
#include "objloader.h"
#include <filesystem>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <fstream>

namespace fs = std::filesystem;

namespace nx {

// Helper to sanitize and resolve texture paths
static std::string resolve_texture_path(const std::string& model_path, std::string texture_path) {
    // Remove quotes
    texture_path.erase(std::remove(texture_path.begin(), texture_path.end(), '\"'), texture_path.end());
    
    fs::path tex_path(texture_path);
    if (tex_path.is_absolute()) {
        return texture_path;
    }
    
    fs::path model_dir = fs::path(model_path).parent_path();
    fs::path resolved = model_dir / tex_path;
    
    if (fs::exists(resolved)) {
        return resolved.string();
    }
    
    return texture_path;
}

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
            throw std::runtime_error("Could not open OBJ file: " + obj_path);
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
                mtl_file = resolve_texture_path(obj_path, mtl_file);
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
            current_material.base_color_texture = resolve_texture_path(mtl_path, tex_path);
        }
        else if (cmd == "map_Ks") {
            std::string tex_path;
            std::getline(iss >> std::ws, tex_path);
            current_material.specular_texture = resolve_texture_path(mtl_path, tex_path);
        }
        else if (cmd == "map_Bump" || cmd == "bump") {
            std::string tex_path;
            std::getline(iss >> std::ws, tex_path);
            current_material.normal_texture = resolve_texture_path(mtl_path, tex_path);
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

void ObjLoader::load(const std::filesystem::path& input_path, MeshFiles& mesh) {
    std::ifstream file(input_path);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open OBJ file: " + input_path.string());
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
    
    // Reserve temp storage
    temp_positions.reserve(position_count);
    temp_normals.reserve(normal_count);
    temp_texcoords.reserve(texcoord_count);
    
    // Second pass: load all vertex attributes
    file.clear();
    file.seekg(0);
    
    Aabb bounds;
    bool first_vertex = true;
    
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
            
            temp_positions.push_back(p);
        }
        else if (cmd == "vn") {
            Vector3f n;
            iss >> n.x >> n.y >> n.z;
            temp_normals.push_back(n);
        }
        else if (cmd == "vt") {
            Vector2f t;
            iss >> t.u >> t.v;
            temp_texcoords.push_back(t);
        }
    }
    
    // Resize mesh arrays
    Index wedge_count = triangle_count * 3;
    mesh.positions.resize(position_count);
    mesh.wedges.resize(wedge_count);
    mesh.triangles.resize(triangle_count);
    
    bool has_materials = !materials.empty();
    if (has_materials) {
        mesh.material_ids.resize(triangle_count);
        mesh.materials = materials;
    }
    
    // Copy positions
    for (Index i = 0; i < position_count; ++i) {
        mesh.positions[i] = temp_positions[i];
    }
    
    // Third pass: build wedges and triangles
    file.clear();
    file.seekg(0);
    
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
                    wedge.p = pos_idx;
                    
                    // Texcoord
                    if (idx[1] != 0 && !temp_texcoords.empty()) {
                        Index tc_idx = (idx[1] > 0) ? idx[1] - 1 : texcoord_count + idx[1];
                        if (tc_idx < texcoord_count) {
                            wedge.t = temp_texcoords[tc_idx];
                        }
                    } else {
                        wedge.t = {0.0f, 0.0f};
                    }
                    
                    // Normal
                    if (idx[2] != 0 && !temp_normals.empty()) {
                        Index n_idx = (idx[2] > 0) ? idx[2] - 1 : normal_count + idx[2];
                        if (n_idx < normal_count) {
                            wedge.n = temp_normals[n_idx];
                        }
                    } else {
                        wedge.n = {0.0f, 0.0f, 0.0f};
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
