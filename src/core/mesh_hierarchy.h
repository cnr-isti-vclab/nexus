#ifndef NX_MESH_HIERARCHY_H
#define NX_MESH_HIERARCHY_H

#include <vector>
#include <memory>
#include "mesh.h"

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
	MeshHierarchy() = default;
	
	void initialize(MeshFiles&& base_mesh);
	void build_hierarchy(const BuildParameters& params);
	
private:
	void process_level(MeshFiles& mesh, MeshFiles& next_mesh, std::size_t clusters_per_node);
	
	std::vector<MeshFiles> levels;
};

} // namespace nx

#endif // NX_MESH_HIERARCHY_H
