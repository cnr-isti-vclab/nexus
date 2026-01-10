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
#include "plyexporter.h"
#include <fstream>
#include <iostream>
#include <cmath>

namespace nx {

// Generate a color based on index using a rainbow colormap
static void index_to_color(Index idx, Index total, uint8_t& r, uint8_t& g, uint8_t& b) {
    float normalized = static_cast<float>(idx) / std::max(1u, total - 1);
    
    // Rainbow colormap
    float h = normalized * 5.0f; // 0 to 5
    int sector = static_cast<int>(h);
    float f = h - sector;
    
    switch (sector % 6) {
        case 0: r = 255; g = static_cast<uint8_t>(f * 255); b = 0; break;
        case 1: r = static_cast<uint8_t>((1 - f) * 255); g = 255; b = 0; break;
        case 2: r = 0; g = 255; b = static_cast<uint8_t>(f * 255); break;
        case 3: r = 0; g = static_cast<uint8_t>((1 - f) * 255); b = 255; break;
        case 4: r = static_cast<uint8_t>(f * 255); g = 0; b = 255; break;
        case 5: r = 255; g = 0; b = static_cast<uint8_t>((1 - f) * 255); break;
    }
}

void export_ply(const MeshFiles& mesh, const std::filesystem::path& output_path, bool color_by_index) {
    std::ofstream file(output_path);
    if (!file.is_open()) {
        throw std::runtime_error("Could not create PLY file: " + output_path.string());
    }
    
    Index num_vertices = mesh.positions.size();
    Index num_faces = mesh.triangles.size();
    
    // Write PLY header
    file << "ply\n";
    file << "format ascii 1.0\n";
    file << "comment Nexus v4 PLY export\n";
    file << "element vertex " << num_vertices << "\n";
    file << "property float x\n";
    file << "property float y\n";
    file << "property float z\n";
    
    if (color_by_index) {
        file << "property uchar red\n";
        file << "property uchar green\n";
        file << "property uchar blue\n";
    }
    
    file << "element face " << num_faces << "\n";
    file << "property list uchar int vertex_indices\n";
    file << "end_header\n";
    
    // Write vertices
    for (Index i = 0; i < num_vertices; ++i) {
        const Vector3f& p = mesh.positions[i];
        file << p.x << " " << p.y << " " << p.z;
        
        if (color_by_index) {
            uint8_t r, g, b;
            index_to_color(i, num_vertices, r, g, b);
            file << " " << static_cast<int>(r) 
                 << " " << static_cast<int>(g) 
                 << " " << static_cast<int>(b);
        }
        
        file << "\n";
    }
    
    // Write faces using unique position indices from wedges
    for (Index i = 0; i < num_faces; ++i) {
        const Triangle& tri = mesh.triangles[i];
        
        file << "3";
        for (int j = 0; j < 3; ++j) {
            Index wedge_idx = tri.w[j];
            const Wedge& w = mesh.wedges[wedge_idx];
            file << " " << w.p;
        }
        file << "\n";
    }
    
    std::cout << "Exported " << num_vertices << " vertices and " 
              << num_faces << " faces to " << output_path << std::endl;
    
    if (color_by_index) {
        std::cout << "Vertices colored by index (rainbow colormap)" << std::endl;
    }
}

} // namespace nx
