#ifndef NX_BASEMESH_H
#define NX_BASEMESH_H

#include <string>
#include <filesystem>
#include <map>

#include "mesh_types.h"
#include "mapped_file.h"
#include "material.h"

namespace nx {

/**
 * Out-of-core mesh storage backed by mapped files in a directory:
 *  - positions.bin     (Vector3f array)
 *  - colors.bin        (Rgba8 array, optional)
 *  - wedges.bin        (Wedge array - position index + normal + texcoord)
 *  - triangles.bin     (Triangle array)
 *  - material_ids.bin  (Index array, per-triangle material, optional)
 *  - adjacency.bin     (FaceAdjacency array)
 *  - clusters.bin      (Cluster array with bounds)
 *  - cluster_vertices.bin (Index array, flat vertex list for all clusters)
 *  - materials.json    (Material array as JSON)
 */

class BaseMesh {
	enum class Component : uint32_t {
		None        = 0,
		Position    = 1 << 0,
		Normal      = 1 << 1,
		Color       = 1 << 2,
		TexCoords   = 1 << 3,
	};


	bool hasComponent(Component comp) const {
		return (components & static_cast<uint32_t>(comp)) != 0;
	}

	void addComponent(Component comp) {
		components |= static_cast<uint32_t>(comp);
	}

private:
	uint32_t components = static_cast<uint32_t>(Component::None);
};

} // namespace nx

#endif // NX_BASEMESH_H
