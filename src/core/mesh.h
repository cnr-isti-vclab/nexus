#ifndef NX_MESH_H
#define NX_MESH_H

#include <string>
#include <filesystem>

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

class MeshFiles {
public:
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
	MappedArray<Wedge> wedges;            // Always present
	MappedArray<Triangle> triangles;      // Always present
	MappedArray<Index> material_ids;      // Optional (size 0 if single/no material)
	MappedArray<FaceAdjacency> adjacency; // Computed separately
	MappedArray<Cluster> clusters;        // Computed by clustering (includes bounds)

	// Triangle to cluster mapping (in-memory, not memory-mapped)
	std::vector<Index> triangle_to_cluster;

	// Micronodes (in-memory, not memory-mapped)
	std::vector<MicroNode> micronodes;

	// Materials stored as JSON (not memory-mapped)
	std::vector<Material> materials;

	// Create empty files; callers typically resize afterwards.
	bool create(const std::filesystem::path& dir);

	void close();


private:
	std::filesystem::path dir;

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
