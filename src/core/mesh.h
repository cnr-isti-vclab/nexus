#ifndef NX_MESH_H
#define NX_MESH_H

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

class Mesh {
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
}

class MeshFiles {
public:
	bool has_colors = false;
	bool has_normals = true;
	bool has_textures = false;

	MeshFiles();
	~MeshFiles();

	MeshFiles(const MeshFiles&) = delete;
	MeshFiles& operator=(const MeshFiles&) = delete;
	MeshFiles(MeshFiles&&) = default;
	MeshFiles& operator=(MeshFiles&&) = default;

	Aabb bounds{{0,0,0}, {0,0,0}};

	// Exposed mapped arrays
	MappedArray<Vector3f> positions;      // Always present
	MappedArray<Rgba8> colors;            // Optional (size 0 if no colors)
	MappedArray<Vector3f> normals;            // Optional (size 0 if no colors)
	MappedArray<Vector2f> texcoords;            // Optional (size 0 if no colors)

	MappedArray<Wedge> wedges;            // Always present

	MappedArray<Triangle> triangles;      // Always present
	MappedArray<Index> material_ids;      // Optional (size 0 if single/no material)

	MappedArray<FaceAdjacency> adjacency; // Computed separately
	MappedArray<Cluster> clusters;        // Computed by clustering (includes bounds)

	// Triangle to cluster mapping (in-memory, not memory-mapped)
	std::vector<Index> triangle_to_cluster;

	// Micronodes (in-memory, not memory-mapped)
	std::vector<MicroNode> micronodes;
	std::vector<NodeTexture> node_textures; // Texture info for each micronode

	std::vector<MacroNode> macronodes;
	
	// Materials stored as JSON (not memory-mapped)
	std::vector<Material> materials;

	MappedArray<uint8_t>texels; //store textures for each micronode.

	// Create empty files; callers typically resize afterwards.
	bool create(const std::filesystem::path& dir);

	void close();


	std::filesystem::path dir;
private:

	std::filesystem::path pathFor(const char* fname) const { return dir / fname; }
	bool mapDataFiles(MappedFile::Mode mode);
};

// Helpers to compute byte sizes from counts.
inline std::size_t positions_bytes(Index n)    { return static_cast<std::size_t>(n) * sizeof(Vector3f); }
inline std::size_t colors_bytes(Index n)       { return static_cast<std::size_t>(n) * sizeof(Rgba8); }
inline std::size_t wedges_bytes(Index n)       { return static_cast<std::size_t>(n) * sizeof(Wedge); }
inline std::size_t triangles_bytes(Index n)    { return static_cast<std::size_t>(n) * sizeof(Triangle); }
inline std::size_t material_ids_bytes(Index n) { return static_cast<std::size_t>(n) * sizeof(Index); }
inline std::size_t adjacency_bytes(Index n)    { return static_cast<std::size_t>(n) * sizeof(FaceAdjacency); }
inline std::size_t clusters_bytes(Index n)     { return static_cast<std::size_t>(n) * sizeof(Cluster); }
inline std::size_t cluster_triangles_bytes(Index n) { return static_cast<std::size_t>(n) * sizeof(Index); }

} // namespace nx

#endif // NX_MESH_H
