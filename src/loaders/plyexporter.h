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
#pragma once

#include "../core/mesh.h"
#include <filesystem>

namespace nx {

// Export mesh to PLY format
// If color_by_index is true, vertices are colored by their index (useful for visualizing spatial ordering)
void export_ply(const MeshFiles& mesh, const std::filesystem::path& output_path, bool color_by_index = false);

} // namespace nx
