#ifndef NX_MAPPEDMESH_H
#define NX_MAPPEDMESH_H

#include "basemesh.h"
#include "mesh_types.h"
#include <mutex>
#include "json.hpp"
#include <fstream>
#include <iomanip>

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

class MappedMesh {
public:
	bool has_colors = false;
	bool has_normals = true;
	bool has_textures = false;

	MappedMesh();
	~MappedMesh();

	std::filesystem::path dir;
	std::mutex lock;
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

	// Triangle to cluster mapping (memory-mapped)
	MappedArray<Index> triangle_to_cluster;

	// Micronodes (in-memory, not memory-mapped)
	std::vector<MicroNode> micronodes;
	// Texture info for each micronode (memory-mapped)
	MappedArray<NodeTexture> node_textures;

	std::vector<MacroNode> macronodes;

	MappedArray<uint8_t>texels; //store textures for each micronode.

	// Create empty files; callers typically resize afterwards.
	bool create(const std::filesystem::path& dir);
	void close();

	// State persistence for resumable builds
	void saveState(const std::filesystem::path& filepath);
	void loadState(const std::filesystem::path& filepath);

	// Allocate node textures and texels for micronodes
	void allocate_node_textures_and_texels(int tex_res, int components);

private:
	bool mapDataFiles(MappedFile::Mode mode);
	std::filesystem::path pathFor(const char* fname) const { return dir / fname; }
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

}
#endif // NX_MAPPEDMESH_H
