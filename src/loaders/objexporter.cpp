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
#include "objexporter.h"
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <iostream>

namespace nx {

void export_obj(const MeshFiles& mesh, const std::filesystem::path& output_path) {
	std::ofstream file(output_path);
	if (!file.is_open()) {
		throw std::runtime_error("Could not create OBJ file: " + output_path.string());
	}

	file << "# Nexus v4 OBJ export (raw dump for verification)\n";
	file << "# Vertices: " << mesh.positions.size() << "\n";
	file << "# Wedges: " << mesh.wedges.size() << "\n";
	file << "# Triangles: " << mesh.triangles.size() << "\n\n";

	// Write positions
	for (Index i = 0; i < mesh.positions.size(); ++i) {
		const Vector3f& p = mesh.positions[i];
		file << "v " << p.x << " " << p.y << " " << p.z << "\n";
	}
	file << "\n";

	// Write texcoords directly from wedges
	for (Index i = 0; i < mesh.wedges.size(); ++i) {
		const Wedge& w = mesh.wedges[i];
		file << "vt " << w.t.u << " " << w.t.v << "\n";
	}
	file << "\n";

	// Write normals directly from wedges
	for (Index i = 0; i < mesh.wedges.size(); ++i) {
		const Wedge& w = mesh.wedges[i];
		file << "vn " << w.n.x << " " << w.n.y << " " << w.n.z << "\n";
	}
	file << "\n";

	// Write materials if present
	bool has_materials = !mesh.materials.empty();
	std::string mtl_filename;

	if (has_materials) {
		mtl_filename = output_path.stem().string() + ".mtl";
		file << "mtllib " << mtl_filename << "\n\n";

		// Export MTL file
		std::filesystem::path mtl_path = output_path.parent_path() / mtl_filename;
		std::ofstream mtl_file(mtl_path);
		if (mtl_file.is_open()) {
			for (size_t i = 0; i < mesh.materials.size(); ++i) {
				const Material& mat = mesh.materials[i];
				mtl_file << "newmtl material_" << i << "\n";
				mtl_file << "Kd " << mat.base_color[0] << " " << mat.base_color[1] << " " << mat.base_color[2] << "\n";
				mtl_file << "d " << mat.base_color[3] << "\n";

				if (!mat.base_color_texture.empty()) {
					mtl_file << "map_Kd " << mat.base_color_texture << "\n";
				}

				if (mat.is_phong()) {
					mtl_file << "Ks " << mat.specular[0] << " " << mat.specular[1] << " " << mat.specular[2] << "\n";
					mtl_file << "Ns " << mat.shininess << "\n";
					if (!mat.specular_texture.empty()) {
						mtl_file << "map_Ks " << mat.specular_texture << "\n";
					}
				} else {
					mtl_file << "Pr " << mat.roughness_factor << "\n";
					mtl_file << "Pm " << mat.metallic_factor << "\n";
				}

				if (!mat.normal_texture.empty()) {
					mtl_file << "bump " << mat.normal_texture << "\n";
				}

				mtl_file << "\n";
			}
		}
	}

	// Write faces
	int32_t current_material = -1;
	for (Index i = 0; i < mesh.triangles.size() && i < 10000; ++i) {
		const Triangle& tri = mesh.triangles[i];

		// Handle material changes
		if (has_materials && mesh.material_ids.size() > i) {
			int32_t mat_id = mesh.material_ids[i];
			if (mat_id != current_material) {
				if (mat_id >= 0) {
					file << "usemtl material_" << mat_id << "\n";
				}
				current_material = mat_id;
			}
		}

		file << "f";
		for (int j = 0; j < 3; ++j) {
			Index wedge_idx = tri.w[j];
			const Wedge& w = mesh.wedges[wedge_idx];

			// Position index (1-based), texcoord index, normal index
			// All reference the wedge arrays directly
			file << " " << (w.p + 1) << "/" << (wedge_idx + 1) << "/" << (wedge_idx + 1);
		}
		file << "\n";
	}

	std::cout << "Exported " << mesh.triangles.size() << " triangles to " << output_path << std::endl;
}

} // namespace nx
