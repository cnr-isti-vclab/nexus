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
#include <vector>
#include <algorithm>

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

// Build a vertex-to-cluster mapping from triangle cluster assignments
static std::vector<Index> build_vertex_to_cluster_map(const MeshFiles& mesh) {
    Index num_vertices = mesh.positions.size();
    std::vector<Index> vertex_to_cluster(num_vertices, std::numeric_limits<Index>::max());
    
    // Iterate through all clusters
    for (Index cluster_idx = 0; cluster_idx < mesh.clusters.size(); ++cluster_idx) {
        const Cluster& cluster = mesh.clusters[cluster_idx];
        
        // Iterate through all triangles in this cluster
        for (Index tri_offset = cluster.triangle_offset; 
             tri_offset < cluster.triangle_offset + cluster.triangle_count; ++tri_offset) {
            // Get the triangle index from the cluster_triangles array
            Index tri_idx = mesh.cluster_triangles[tri_offset];
            const Triangle& tri = mesh.triangles[tri_idx];
            
            // Mark all vertices of this triangle as belonging to this cluster
            for (int j = 0; j < 3; ++j) {
                Index vertex_idx = mesh.wedges[tri.w[j]].p;
                // Assign to first cluster that claims this vertex
                if (vertex_to_cluster[vertex_idx] == std::numeric_limits<Index>::max()) {
                    vertex_to_cluster[vertex_idx] = cluster_idx;
                }
            }
        }
    }
    
    return vertex_to_cluster;
}

void export_ply(const MeshFiles& mesh, 
                const std::filesystem::path& output_path, 
                ColoringMode coloring_mode) {
    std::ofstream file(output_path);
    if (!file.is_open()) {
        throw std::runtime_error("Could not create PLY file: " + output_path.string());
    }
    
    Index num_vertices = mesh.positions.size();
    Index num_faces = mesh.triangles.size();
    
    // Validate coloring mode
    bool has_colors = (coloring_mode != ColoringMode::None);
    if (coloring_mode == ColoringMode::ByCluster && mesh.clusters.size() == 0) {
        std::cerr << "Warning: No clusters available for ByCluster coloring, using no coloring" << std::endl;
        has_colors = false;
    }
    
    // Pre-compute vertex-to-cluster mapping if needed
    std::vector<Index> vertex_to_cluster;
    if (coloring_mode == ColoringMode::ByCluster) {
        vertex_to_cluster = build_vertex_to_cluster_map(mesh);
    }
    
    // Write PLY header
    file << "ply\n";
    file << "format ascii 1.0\n";
    file << "comment Nexus v4 PLY export\n";
    file << "element vertex " << num_vertices << "\n";
    file << "property float x\n";
    file << "property float y\n";
    file << "property float z\n";
    
    if (has_colors) {
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
        
        if (has_colors) {
            uint8_t r, g, b;
            
            switch (coloring_mode) {
                case ColoringMode::ByVertexIndex:
                    index_to_color(i, num_vertices, r, g, b);
                    break;
                    
                case ColoringMode::ByCluster:
                    if (vertex_to_cluster[i] != std::numeric_limits<Index>::max()) {
                        // Pseudo-random color for each cluster
                        uint32_t seed = vertex_to_cluster[i] * 2654435761u;
                        seed ^= seed >> 16;
                        seed *= 0x85ebca6b;
                        seed ^= seed >> 13;
                        r = static_cast<uint8_t>((seed >> 0) & 0xFF);
                        g = static_cast<uint8_t>((seed >> 8) & 0xFF);
                        b = static_cast<uint8_t>((seed >> 16) & 0xFF);
                    } else {
                        r = g = b = 128; // Gray for unassigned vertices
                    }
                    break;
                    
                case ColoringMode::ByMacroNode:
                    // Placeholder for macro-node coloring
                    r = g = b = 200; // Light gray placeholder
                    break;
                    
                default:
                    r = g = b = 255; // White
            }
            
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
    
    switch (coloring_mode) {
        case ColoringMode::ByVertexIndex:
            std::cout << "Vertices colored by index (rainbow colormap)" << std::endl;
            break;
        case ColoringMode::ByCluster:
            std::cout << "Vertices colored by cluster (" << mesh.clusters.size() << " clusters)" << std::endl;
            break;
        case ColoringMode::ByMacroNode:
            std::cout << "Vertices colored by macro-node (placeholder)" << std::endl;
            break;
        case ColoringMode::None:
            std::cout << "No vertex coloring" << std::endl;
            break;
    }
}

} // namespace nx
