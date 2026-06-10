#ifndef NX_MESH_HIERARCHY_H
#define NX_MESH_HIERARCHY_H

#include <vector>
#include <memory>
#include "mappedmesh.h"
#include "material.h"

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
	~MeshHierarchy();
	MeshHierarchy(const MeshHierarchy&) = delete;
	MeshHierarchy& operator=(const MeshHierarchy&) = delete;

	std::vector<MappedMesh *> levels;
	std::vector<Material> materials;
	std::vector<Material::TextureSlot> texture_slots; //which maps are stored in texels one after the other.

	void initialize(MappedMesh *base_mesh, std::vector<Material> &_materials);
	void build_hierarchy(const BuildParameters& params);

	// Resume hierarchy levels from a `levels.json` file
	void resumeFromLevelsJson(const std::filesystem::path& levels_path = "levels.json");
	
private:
	void process_level(MappedMesh& mesh, MappedMesh& next_mesh, const BuildParameters &params);
};

} // namespace nx

#endif // NX_MESH_HIERARCHY_H
