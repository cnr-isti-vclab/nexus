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
#include "../core/mesh_types.h"
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
	if (coloring_mode == ColoringMode::ByCluster && mesh.clusters.size() == 0) {
		throw std::runtime_error("Cannot export with ByCluster coloring: no clusters available in mesh");
	}

	if (coloring_mode == ColoringMode::ByMicroNode && mesh.micronodes.empty()) {
		throw std::runtime_error("Cannot export with ByMicroNode coloring: no micronodes available in mesh");
	}

	bool has_colors = (coloring_mode != ColoringMode::None);

	// Write PLY header
	file << "ply\n";
	file << "format ascii 1.0\n";
	file << "comment Nexus v4 PLY export\n";
	file << "element vertex " << num_vertices << "\n";
	file << "property float x\n";
	file << "property float y\n";
	file << "property float z\n";

	file << "element face " << num_faces << "\n";
	file << "property list uchar int vertex_indices\n";

	if (has_colors) {
		file << "property uchar red\n";
		file << "property uchar green\n";
		file << "property uchar blue\n";
	}

	file << "end_header\n";

	// Write vertices (no colors - they'll be on faces)
	for (Index i = 0; i < num_vertices; ++i) {
		const Vector3f& p = mesh.positions[i];
		file << p.x << " " << p.y << " " << p.z << "\n";
	}

	// Write faces with colors
	if (coloring_mode == ColoringMode::ByCluster) {
		// Iterate clusters and output their triangles with cluster colors
		for (Index c = 0; c < mesh.clusters.size(); ++c) {
			const Cluster& cluster = mesh.clusters[c];

			// Compute color for this cluster
			uint32_t seed = c * 2654435761u;
			seed ^= seed >> 16;
			seed *= 0x85ebca6b;
			seed ^= seed >> 13;
			uint8_t r = static_cast<uint8_t>((seed >> 0) & 0xFF);
			uint8_t g = static_cast<uint8_t>((seed >> 8) & 0xFF);
			uint8_t b = static_cast<uint8_t>((seed >> 16) & 0xFF);
			r = r/2 + 128;
			g = g/2 + 128;
			b = b/2 + 128;

			// Output all triangles in this cluster with this color
			for (Index tri_idx = cluster.triangle_offset;
				 tri_idx < cluster.triangle_offset + cluster.triangle_count; ++tri_idx) {
				const Triangle& tri = mesh.triangles[tri_idx];

				file << "3";
				for (int j = 0; j < 3; ++j) {
					Index wedge_idx = tri.w[j];
					const Wedge& w = mesh.wedges[wedge_idx];
					file << " " << w.p;
				}

				file << " " << static_cast<int>(r)
					 << " " << static_cast<int>(g)
					 << " " << static_cast<int>(b) << "\n";
			}
		}
	} else if (coloring_mode == ColoringMode::ByMicroNode) {
		// Iterate micronodes and their clusters
		for (Index mn_idx = 0; mn_idx < mesh.micronodes.size(); ++mn_idx) {
			const MicroNode& micronode = mesh.micronodes[mn_idx];

			// Compute random base hue for this micronode
			uint32_t seed = mn_idx * 2654435761u;
			seed ^= seed >> 16;
			seed *= 0x85ebca6b;
			seed ^= seed >> 13;
			uint8_t r = static_cast<uint8_t>((seed >> 0) & 0xFF);
			uint8_t g = static_cast<uint8_t>((seed >> 8) & 0xFF);
			uint8_t b = static_cast<uint8_t>((seed >> 16) & 0xFF);
			r = r/2 + 128;
			g = g/2 + 128;
			b = b/2 + 128;

			// Iterate over clusters in this micronode
			for (std::size_t local_c = 0; local_c < micronode.cluster_ids.size(); ++local_c) {
				Index cluster_id = micronode.cluster_ids[local_c];
				const Cluster& cluster = mesh.clusters[cluster_id];

				// Output all triangles in this cluster
				for (Index tri_idx = cluster.triangle_offset;
					 tri_idx < cluster.triangle_offset + cluster.triangle_count; ++tri_idx) {
					const Triangle& tri = mesh.triangles[tri_idx];

					file << "3";
					for (int j = 0; j < 3; ++j) {
						Index wedge_idx = tri.w[j];
						const Wedge& w = mesh.wedges[wedge_idx];
						file << " " << w.p;
					}

					file << " " << static_cast<int>(r)
						 << " " << static_cast<int>(g)
						 << " " << static_cast<int>(b) << "\n";
				}
			}
		}
	} else {
		// Iterate all faces for other coloring modes
		for (Index tri_idx = 0; tri_idx < num_faces; ++tri_idx) {
			const Triangle& tri = mesh.triangles[tri_idx];

			file << "3";
			for (int j = 0; j < 3; ++j) {
				Index wedge_idx = tri.w[j];
				const Wedge& w = mesh.wedges[wedge_idx];
				file << " " << w.p;
			}

			if (coloring_mode != ColoringMode::None) {
				uint8_t r, g, b;

				switch (coloring_mode) {
				case ColoringMode::ByVertexIndex:
					index_to_color(tri_idx, num_faces, r, g, b);
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
	}

	std::cout << "Exported " << num_vertices << " vertices and "
			  << num_faces << " faces to " << output_path << std::endl;

	switch (coloring_mode) {
	case ColoringMode::ByVertexIndex:
		std::cout << "Faces colored by triangle index (rainbow colormap)" << std::endl;
		break;
	case ColoringMode::ByCluster:
		std::cout << "Faces colored by cluster (" << mesh.clusters.size() << " clusters)" << std::endl;
		break;
	case ColoringMode::ByMicroNode:
		std::cout << "Faces colored by micronode (" << mesh.micronodes.size() << " micronodes)" << std::endl;
		break;
	case ColoringMode::ByMacroNode:
		std::cout << "Faces colored by macro-node (placeholder)" << std::endl;
		break;
	case ColoringMode::None:
		std::cout << "No face coloring" << std::endl;
		break;
	}
}

} // namespace nx
