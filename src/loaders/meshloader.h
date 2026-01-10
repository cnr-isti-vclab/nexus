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
#ifndef NX_LOADERS_MESHLOADER_H
#define NX_LOADERS_MESHLOADER_H

#include "../core/mesh_types.h"
#include "../core/mesh.h"
#include "../core/material.h"
#include <vector>
#include <string>
#include <filesystem>

namespace nx {

/**
 * Base class for mesh loaders (OBJ, PLY, glTF, etc.)
 * Populates a MeshFiles structure with indexed geometry backed by mmapped files.
 * 
 * Philosophy:
 *  - Load source formats (which may have per-face-corner attributes)
 *  - Populate positions array (spatial vertices)
 *  - Create wedges (position_index + normal + uv per corner)
 *  - Build triangles (3 wedge indices per face)
 *  - Optionally populate colors array (if vertex colors present)
 *  - Adjacency is NOT computed by loader (done separately)
 */
// Simple dispatcher: detect format from extension and call the appropriate loader.
// Throws exception on error or unsupported format.
void load_mesh(const std::filesystem::path& input_path, MeshFiles& mesh);

} // namespace nx

#endif // NX_LOADERS_MESHLOADER_H

