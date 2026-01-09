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
#ifndef NX_OBJLOADER_H
#define NX_OBJLOADER_H

#include "meshloader.h"
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace nx {

class ObjLoader {
public:
    ObjLoader(const std::string& filename, const std::string& mtl_path = "");
    
    bool load(const std::filesystem::path& input_path, MeshFiles& mesh);
    
private:
    void read_mtls();
    void read_mtl(const std::string& mtl_path);
    
    std::string obj_path;
    bool use_custom_mtl = false;
    std::vector<std::string> mtl_files;
    
    // Temporary storage during parsing
    std::vector<Vector3f> temp_positions;
    std::vector<Vector3f> temp_normals;
    std::vector<Vector2f> temp_texcoords;
    
    // Materials
    std::vector<Material> materials;
    std::unordered_map<std::string, int32_t> material_map;
    int32_t current_material_id = -1;
};

} // namespace nx

#endif // NX_OBJLOADER_H
