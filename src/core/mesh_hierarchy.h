#ifndef NX_MESH_HIERARCHY_H
#define NX_MESH_HIERARCHY_H

#include <vector>
#include <memory>
#include "mappedmesh.h"

namespace nx {

// Forward declaration
struct BuildParameters;

/**
 * MeshHierarchy manages the creation and storage of multiple LOD levels
 * during hierarchical mesh simplification.
 * 
 */
class MeshHierarchy {
public:
	std::vector<MappedMesh> levels;

	MeshHierarchy() = default;
	
	void initialize(MappedMesh&& base_mesh);
	void build_hierarchy(const BuildParameters& params);
	
private:
	void process_level(MappedMesh& mesh, MappedMesh& next_mesh, const BuildParameters &params);
};

} // namespace nx

#endif // NX_MESH_HIERARCHY_H
