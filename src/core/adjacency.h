#pragma once

#include "mesh.h"
#include <vector>

namespace nx {

// Compute face-face adjacency from triangles
// Handles non-manifold geometry by treating edges with >2 incident faces as borders
// Returns adjacency array where adjacency[face].opp[corner] = adjacent_face or UINT32_MAX if border
void compute_adjacency(MeshFiles& mesh);

} // namespace nx
