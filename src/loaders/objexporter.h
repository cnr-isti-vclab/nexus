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
#ifndef NX_OBJEXPORTER_H
#define NX_OBJEXPORTER_H

#include "../core/mappedmesh.h"
#include "../core/nodemesh.h"
#include "../core/material.h"
#include <filesystem>
#include <vector>

namespace nx {

// Export MappedMesh to OBJ format (for testing/validation)
void export_obj(const MappedMesh& mesh, const std::vector<Material>& materials, const std::filesystem::path& output_path);

// Export NodeMesh + TileMap to OBJ/MTL + PNG textures.
void export_iobj(const NodeMesh& mesh, const std::vector<Material>& materials, const std::filesystem::path& output_path);

} // namespace nx

#endif // NX_OBJEXPORTER_H
