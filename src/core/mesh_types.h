#ifndef NX_MESH_TYPES_H
#define NX_MESH_TYPES_H

#include <cmath>
#include <cstdint>
#include <type_traits>
#include <vector>
#include <limits>

// Compact geometry types intended for out-of-core usage.
// Keep their data members packed and layouts stable for mapped I/O.

namespace nx {

struct Vector3f { float x, y, z;
	float &operator[](std::size_t i) { return i == 0 ? x : (i == 1 ? y : z); }
	const float &operator[](std::size_t i) const { return i == 0 ? x : (i == 1 ? y : z); }
	bool operator==(const Vector3f &v) const { return x == v.x && y == v.y && z == v.z; }
	Vector3f operator+(const Vector3f &v) const { return {x + v.x, y + v.y, z + v.z}; }
	Vector3f operator-(const Vector3f &v) const { return {x - v.x, y - v.y, z - v.z}; }
	Vector3f operator-() const { return {-x, -y, -z}; }
	Vector3f operator*(float s) const { return {x * s, y * s, z * s}; }
	Vector3f operator/(float s) const { return {x / s, y / s, z / s}; }
	Vector3f &operator+=(const Vector3f &v) { x += v.x; y += v.y; z += v.z; return *this; }
	Vector3f &operator-=(const Vector3f &v) { x -= v.x; y -= v.y; z -= v.z; return *this; }
	Vector3f &operator*=(float s) { x *= s; y *= s; z *= s; return *this; }
	Vector3f &operator/=(float s) { x /= s; y /= s; z /= s; return *this; }
	float dot(const Vector3f &v) const { return x * v.x + y * v.y + z * v.z; }
	float norm() const { return std::sqrt(dot(*this)); }
	float squaredNorm() const { return dot(*this); }
};
struct Vector3d { double x, y, z; };
struct Vector3s {
	std::int16_t x, y, z;
	std::int16_t &operator[](std::size_t i) { return i == 0 ? x : (i == 1 ? y : z); }
	const std::int16_t &operator[](std::size_t i) const { return i == 0 ? x : (i == 1 ? y : z); }
};
struct Vector2f {
	float u, v;
	float &operator[](std::size_t i) { return i == 0 ? u : v; }
	const float &operator[](std::size_t i) const { return i == 0 ? u : v; }
};

// Infinite ray; direction is not necessarily normalized.
class Ray3 {
public:
	Ray3() = default;
	Ray3(const Vector3f &origin, const Vector3f &direction): origin_(origin), direction_(direction) {}

	const Vector3f &Origin() const { return origin_; }
	Vector3f &Origin() { return origin_; }
	const Vector3f &Direction() const { return direction_; }
	void SetOrigin(const Vector3f &origin) { origin_ = origin; }
	void SetDirection(const Vector3f &direction) { direction_ = direction; }
	bool NormalizeDirection() {
		float length = direction_.norm();
		if(length == 0.0f) return false;
		direction_ /= length;
		return true;
	}

private:
	Vector3f origin_{};
	Vector3f direction_{};
};

using Ray3f = Ray3;

struct Rgba8 {
	std::uint8_t r, g, b, a;
	std::uint8_t &operator[](std::size_t i) { return i == 0 ? r : (i == 1 ? g : (i == 2 ? b : a)); }
	const std::uint8_t &operator[](std::size_t i) const { return i == 0 ? r : (i == 1 ? g : (i == 2 ? b : a)); }
};

// Axis-aligned bounding box used for mesh extents.
class Aabb {
public:
    Vector3f min;
    Vector3f max;
};


// Bounding sphere; radius < 0 means empty.
struct Sphere {
	Vector3f center{0, 0, 0};
	float radius = -1;
	Sphere() = default;
	Sphere(const Vector3f &c, float r): center(c), radius(r) {}
	Vector3f &Center() { return center; }
	const Vector3f &Center() const { return center; }
	float &Radius() { return radius; }
	float Radius() const { return radius; }
	bool IsIn(const Sphere &s) const {
		return s.radius < 0 || (radius >= 0 && (s.center - center).norm() + s.radius <= radius);
	}

	// Grows this sphere to contain s.
	void add(const Sphere &s) {
		if(s.radius < 0) return;
		if(radius < 0) { *this = s; return; }
		Vector3f d = s.center - center;
		float dist = d.norm();
		if(dist + s.radius <= radius) return;
		if(dist + radius <= s.radius) { *this = s; return; }
		float r = (dist + radius + s.radius) * 0.5f;
		center += d * ((r - radius) / dist);
		radius = r;
	}
};

// 4x4 matrix, column-major (same layout as OpenGL): element (row, col) is m[col*4 + row].
struct Matrix4f {
	float m[16];

	static Matrix4f identity() {
		return {{1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1}};
	}
	static Matrix4f translation(const Vector3f &t) {
		return {{1,0,0,0, 0,1,0,0, 0,0,1,0, t.x,t.y,t.z,1}};
	}
	static Matrix4f scale(float s) {
		return {{s,0,0,0, 0,s,0,0, 0,0,s,0, 0,0,0,1}};
	}
	static Matrix4f rotationX(float a) {
		float c = std::cos(a), s = std::sin(a);
		return {{1,0,0,0, 0,c,s,0, 0,-s,c,0, 0,0,0,1}};
	}
	static Matrix4f rotationY(float a) {
		float c = std::cos(a), s = std::sin(a);
		return {{c,0,-s,0, 0,1,0,0, s,0,c,0, 0,0,0,1}};
	}

	Matrix4f operator*(const Matrix4f &o) const {
		Matrix4f r;
		for(int c = 0; c < 4; c++)
			for(int row = 0; row < 4; row++) {
				float v = 0;
				for(int k = 0; k < 4; k++)
					v += m[k*4 + row] * o.m[c*4 + k];
				r.m[c*4 + row] = v;
			}
		return r;
	}
	Matrix4f &operator*=(const Matrix4f &o) { return *this = *this * o; }

	// Transforms a point (w = 1).
	Vector3f operator*(const Vector3f &p) const {
		return {m[0]*p.x + m[4]*p.y + m[8]*p.z  + m[12],
				m[1]*p.x + m[5]*p.y + m[9]*p.z  + m[13],
				m[2]*p.x + m[6]*p.y + m[10]*p.z + m[14]};
	}
};

// Indices are 32-bit for now (fits ~4B elements).
using Index = std::uint32_t;
constexpr Index NONE = std::numeric_limits<Index>::max();

// A Wedge combines a reference to a spatial position with unique attributes
// (Normal, Texture Coordinate) for that specific corner of a triangle.
struct Wedge {
	Index p = NONE;      // Index of the position (vertex coordinates)
	Index n = NONE;      // Index of the normal
	Index t = NONE;      // Index of the texture coordinates
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
	Index node = 0xFFFFFFFF;// Stores the node it was created from.
};

// A MicroNode groups clusters at the first level of the hierarchy.
// It contains references to the clusters that belong to it.
struct MicroNode {
    Index id;                           // MicroNode index
    std::vector<Index> cluster_ids;     // Cluster indices belonging to this micronode
    Index triangle_count;               // Total number of triangles across all clusters
    Index vertex_count;
    Vector3f centroid;                  // Weighted spatial center
    Vector3f center;                    // Bounding sphere center
    float radius;                       // Bounding sphere radius
    float error;
};

//the material defines how many components and which textures are stored here.
struct NodeTexture {
    size_t offset = 0; // Offset into the texels array in MeshFiles
    int width = 0;
    int height = 0;
    int components = 3;
    int mip_count = 1;
};

inline std::size_t node_texture_bytes(const NodeTexture& t) {
    return static_cast<std::size_t>(t.width) * static_cast<std::size_t>(t.height) * static_cast<std::size_t>(t.components);
}

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
static_assert(sizeof(Vector3s) == 6,  "Vector3s must stay packed");
static_assert(sizeof(Vector2f) == 8,  "Vector2f must stay packed");
static_assert(sizeof(Rgba8) == 4,     "Rgba8 layout changed unexpectedly");
static_assert(sizeof(Sphere) == 16,   "Sphere layout changed unexpectedly");
static_assert(sizeof(Matrix4f) == 64, "Matrix4f layout changed unexpectedly");
static_assert(sizeof(Aabb) == 24,     "Aabb layout changed unexpectedly");
static_assert(sizeof(Wedge)   == 12, "Wedge layout changed unexpectedly");
static_assert(sizeof(Triangle)== 12, "Triangle layout changed unexpectedly");
static_assert(sizeof(FaceAdjacency)== 12, "FaceAdjacency layout changed unexpectedly");
static_assert(sizeof(HalfedgeRecord) == 16, "HalfedgeRecord layout changed unexpectedly");
static_assert(sizeof(Cluster) == 28, "Cluster layout changed unexpectedly");

} // namespace nx

#endif
