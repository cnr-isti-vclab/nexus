#ifndef NX_MESH_TYPES_H
#define NX_MESH_TYPES_H

#include <cstdint>
#include <type_traits>
#include <vector>

// Basic POD geometry types intended for out-of-core usage.
// Keep them trivial (no constructors) to simplify mmap'ed I/O.

namespace nx {

struct Vector3f { float x, y, z; };
struct Vector2f { float u, v; };
struct Rgba8 { std::uint8_t r, g, b, a; };

// Axis-aligned bounding box used for mesh extents.
class Aabb {
public:
    Vector3f min;
    Vector3f max;
};


// Indices are 32-bit for now (fits ~4B elements).
using Index = std::uint32_t;

// A Wedge combines a reference to a spatial position with unique attributes
// (Normal, Texture Coordinate) for that specific corner of a triangle.
struct Wedge {
    Index p;      // Index of the position (vertex coordinates)
    Vector3f n;   // Normal vector
    Vector2f t;   // Texture coordinates (UV)
};

struct Triangle {
    Index w[3];   // Indices of the 3 wedges forming the triangle
};

// Per-face adjacency: for each corner, the opposite face index (or UINT32_MAX if border).
struct FaceAdjacency {
    Index opp[3];
};

// Halfedge record used for building face-face adjacency out-of-core.
// v0->v1 follows the triangle corner order; face is the owning triangle index.
struct HalfedgeRecord {
    Index v0;
    Index v1;
    Index face;
    Index corner; // 0,1,2 corner within the face
};

// A Cluster groups triangles with a limited triangle count.
// Includes bounding sphere for frustum and cone culling.
// Similar to meshoptimizer's meshopt_Bounds structure.
struct Cluster {
    Index triangle_offset;  // Offset into cluster_triangles array (indices into mesh triangles)
    Index triangle_count;   // Number of triangles in this cluster
    Vector3f center;        // Bounding sphere center
    float radius;           // Bounding sphere radius
};

// A MicroNode groups clusters at the first level of the hierarchy.
// It contains references to the clusters that belong to it.
struct MicroNode {
    Index id;                           // MicroNode index
    std::vector<Index> cluster_ids;     // Cluster indices belonging to this micronode
    Index triangle_count;               // Total number of triangles across all clusters
    Vector3f centroid;                  // Weighted spatial center
    Vector3f center;                    // Bounding sphere center
    float radius;                       // Bounding sphere radius
};

// A MacroNode groups micronodes at the second level of the hierarchy.
// It contains references to the micronodes that belong to it.
struct MacroNode {
    Index id;                           // MacroNode index
    std::vector<Index> micronode_ids;   // MicroNode indices belonging to this macronode
    Index triangle_count;               // Total number of triangles across all micronodes
    Vector3f centroid;                  // Weighted spatial center
    Vector3f center;                    // Bounding sphere center
    float radius;                       // Bounding sphere radius
};

// Macro-node partition result (CSR-like adjacency structure for macro-nodes).
// For now, stores the mapping of micro-nodes to macro-nodes.
struct MacroPartition {
    std::vector<Index> micro_to_macro;  // micro_to_macro[i] = macro-node ID for micro-node i
    Index num_macro_nodes;              // Number of macro-nodes created
};

static_assert(sizeof(Vector3f) == 12, "Vector3f must stay packed");
static_assert(sizeof(Vector2f) == 8,  "Vector2f must stay packed");
static_assert(sizeof(Rgba8) == 4,     "Rgba8 layout changed unexpectedly");
static_assert(sizeof(Aabb) == 24,     "Aabb layout changed unexpectedly");
static_assert(sizeof(Wedge)   == 24, "Wedge layout changed unexpectedly");
static_assert(sizeof(Triangle)== 12, "Triangle layout changed unexpectedly");
static_assert(sizeof(FaceAdjacency)== 12, "FaceAdjacency layout changed unexpectedly");
static_assert(sizeof(HalfedgeRecord) == 16, "HalfedgeRecord layout changed unexpectedly");
static_assert(sizeof(Cluster) == 24, "Cluster layout changed unexpectedly");

} // namespace nx

#endif
