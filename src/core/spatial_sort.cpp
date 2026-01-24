#include "spatial_sort.h"
#include <algorithm>
#include <iostream>

// TODO: In the future we will use mmapped files for out-of-core sorting.
// In that case we should sort the vertices and the indices at the same time.

namespace nx {

// Expand a 21-bit integer into 63 bits by inserting 2 zeros after each bit
static uint64_t expand_bits_21(uint32_t v) {
	uint64_t x = v & 0x1fffff;  // Only keep 21 bits
	x = (x | x << 32) & 0x1f00000000ffffull;
	x = (x | x << 16) & 0x1f0000ff0000ffull;
	x = (x | x << 8)  & 0x100f00f00f00f00full;
	x = (x | x << 4)  & 0x10c30c30c30c30c3ull;
	x = (x | x << 2)  & 0x1249249249249249ull;
	return x;
}

uint64_t morton_code(float x, float y, float z) {
	// Clamp to [0, 1] and quantize to 21 bits
	x = std::max(0.0f, std::min(1.0f, x));
	y = std::max(0.0f, std::min(1.0f, y));
	z = std::max(0.0f, std::min(1.0f, z));

	uint32_t xx = static_cast<uint32_t>(x * 2097151.0f); // 2^21 - 1
	uint32_t yy = static_cast<uint32_t>(y * 2097151.0f);
	uint32_t zz = static_cast<uint32_t>(z * 2097151.0f);

	// Interleave 21 bits per axis into 63-bit Morton code
	uint64_t code = 0;
	code |= expand_bits_21(xx);
	code |= expand_bits_21(yy) << 1;
	code |= expand_bits_21(zz) << 2;

	return code;
}

// Pair of (morton_code, original_index)
struct MortonPair {
	uint64_t code;
	Index index;

	bool operator<(const MortonPair& other) const {
		return code < other.code;
	}
};

// Merge two sorted ranges
static void merge_ranges(
	std::vector<MortonPair>& pairs,
	size_t left, size_t mid, size_t right,
	std::vector<MortonPair>& temp)
{
	size_t i = left;
	size_t j = mid;
	size_t k = 0;

	while (i < mid && j < right) {
		if (pairs[i] < pairs[j]) {
			temp[k++] = pairs[i++];
		} else {
			temp[k++] = pairs[j++];
		}
	}

	while (i < mid) temp[k++] = pairs[i++];
	while (j < right) temp[k++] = pairs[j++];

	// Copy back
	for (k = 0; k < right - left; ++k) {
		pairs[left + k] = temp[k];
	}
}

// Iterative merge sort
static void merge_sort(std::vector<MortonPair>& pairs) {
	size_t n = pairs.size();
	if (n <= 1) return;

	std::vector<MortonPair> temp(n);

	// Bottom-up merge sort
	for (size_t width = 1; width < n; width *= 2) {
		for (size_t left = 0; left < n; left += 2 * width) {
			size_t mid = std::min(left + width, n);
			size_t right = std::min(left + 2 * width, n);

			if (mid < right) {
				merge_ranges(pairs, left, mid, right, temp);
			}
		}
	}
}

std::vector<Index> spatial_sort_positions(MeshFiles& mesh) {
	Index num_positions = mesh.positions.size();
	std::cout << "Spatial sorting " << num_positions << " positions..." << std::endl;

	if (num_positions == 0) {
		return std::vector<Index>();
	}

	// Normalize positions to [0, 1]
	float size_x = mesh.bounds.max.x - mesh.bounds.min.x;
	float size_y = mesh.bounds.max.y - mesh.bounds.min.y;
	float size_z = mesh.bounds.max.z - mesh.bounds.min.z;
	float max_dim = std::max({size_x, size_y, size_z});
	if (max_dim < 1e-6f) max_dim = 1.0f;

	// Compute Morton codes for all positions
	std::vector<MortonPair> pairs;
	pairs.reserve(num_positions);

	for (Index i = 0; i < num_positions; ++i) {
		const Vector3f& p = mesh.positions[i];

		// Normalize to [0, 1]
		float nx = (p.x - mesh.bounds.min.x) / max_dim;
		float ny = (p.y - mesh.bounds.min.y) / max_dim;
		float nz = (p.z - mesh.bounds.min.z) / max_dim;

		MortonPair pair;
		pair.code = morton_code(nx, ny, nz);
		pair.index = i;
		pairs.push_back(pair);
	}

	merge_sort(pairs);

	// Build remapping array: remap[old_index] = new_index
	std::vector<Index> remap(num_positions);
	std::vector<Vector3f> sorted_positions(num_positions);

	for (Index new_idx = 0; new_idx < num_positions; ++new_idx) {
		Index old_idx = pairs[new_idx].index;
		remap[old_idx] = new_idx;
		sorted_positions[new_idx] = mesh.positions[old_idx];
	}

	// Write sorted positions back
	std::cout << "Writing sorted positions..." << std::endl;
	for (Index i = 0; i < num_positions; ++i) {
		mesh.positions[i] = sorted_positions[i];
	}

	std::cout << "Spatial sort complete." << std::endl;
	return remap;
}

void remap_wedge_positions(MeshFiles& mesh, const std::vector<Index>& remap) {
	if (remap.empty()) return;

	std::cout << "Remapping wedge positions..." << std::endl;

	Index num_wedges = mesh.wedges.size();
	for (Index i = 0; i < num_wedges; ++i) {
		Wedge& w = mesh.wedges[i];
		if (w.p < remap.size()) {
			w.p = remap[w.p];
		}
	}

	std::cout << "Remapping complete." << std::endl;
}

std::vector<Index> sort_wedges_by_position(MeshFiles& mesh) {
	Index num_wedges = mesh.wedges.size();
	std::cout << "Sorting " << num_wedges << " wedges by position index..." << std::endl;

	if (num_wedges == 0) {
		return std::vector<Index>();
	}

	// Create index array for sorting
	std::vector<Index> indices(num_wedges);
	for (Index i = 0; i < num_wedges; ++i) {
		indices[i] = i;
	}

	// Sort indices by wedge position
	std::sort(indices.begin(), indices.end(), [&mesh](Index a, Index b) {
		return mesh.wedges[a].p < mesh.wedges[b].p;
	});

	// Build remapping array and sorted wedges
	std::vector<Index> remap(num_wedges);
	std::vector<Wedge> sorted_wedges(num_wedges);

	for (Index new_idx = 0; new_idx < num_wedges; ++new_idx) {
		Index old_idx = indices[new_idx];
		remap[old_idx] = new_idx;
		sorted_wedges[new_idx] = mesh.wedges[old_idx];
	}

	// Write sorted wedges back
	for (Index i = 0; i < num_wedges; ++i) {
		mesh.wedges[i] = sorted_wedges[i];
	}

	std::cout << "Wedge sorting complete." << std::endl;
	return remap;
}

void remap_triangle_wedges(MeshFiles& mesh, const std::vector<Index>& remap) {
	if (remap.empty()) return;

	std::cout << "Remapping triangle wedge indices..." << std::endl;

	Index num_triangles = mesh.triangles.size();
	for (Index i = 0; i < num_triangles; ++i) {
		Triangle& tri = mesh.triangles[i];
		for (int j = 0; j < 3; ++j) {
			if (tri.w[j] < remap.size()) {
				tri.w[j] = remap[tri.w[j]];
			}
		}
	}

	std::cout << "Triangle wedge remapping complete." << std::endl;
}

void sort_triangles_by_wedge(MeshFiles& mesh) {
	Index num_triangles = mesh.triangles.size();
	std::cout << "Sorting " << num_triangles << " triangles by smallest wedge index..." << std::endl;

	if (num_triangles == 0) {
		return;
	}

	// Create index array for sorting
	std::vector<Index> indices(num_triangles);
	for (Index i = 0; i < num_triangles; ++i) {
		indices[i] = i;
	}

	// Sort indices by minimum wedge index in triangle
	std::sort(indices.begin(), indices.end(), [&mesh](Index a, Index b) {
		const Triangle& ta = mesh.triangles[a];
		const Triangle& tb = mesh.triangles[b];
		Index min_a = std::min({ta.w[0], ta.w[1], ta.w[2]});
		Index min_b = std::min({tb.w[0], tb.w[1], tb.w[2]});
		return min_a < min_b;
	});

	// Build sorted triangles
	std::vector<Triangle> sorted_triangles(num_triangles);
	for (Index new_idx = 0; new_idx < num_triangles; ++new_idx) {
		Index old_idx = indices[new_idx];
		sorted_triangles[new_idx] = mesh.triangles[old_idx];
	}

	// Write sorted triangles back
	for (Index i = 0; i < num_triangles; ++i) {
		mesh.triangles[i] = sorted_triangles[i];
	}

	// Also remap material_ids if present
	if (mesh.material_ids.size() == num_triangles) {
		std::vector<Index> sorted_materials(num_triangles);
		for (Index new_idx = 0; new_idx < num_triangles; ++new_idx) {
			Index old_idx = indices[new_idx];
			sorted_materials[new_idx] = mesh.material_ids[old_idx];
		}
		for (Index i = 0; i < num_triangles; ++i) {
			mesh.material_ids[i] = sorted_materials[i];
		}
	}

	std::cout << "Triangle sorting complete." << std::endl;
}

void spatial_sort_mesh(MeshFiles& mesh) {
	// Sort the vertices spatially and reindex
	std::vector<Index> position_remap = spatial_sort_positions(mesh);
	remap_wedge_positions(mesh, position_remap);
	return;

	// Sort wedges by position index and reindex triangles
	std::vector<Index> wedge_remap = sort_wedges_by_position(mesh);
	remap_triangle_wedges(mesh, wedge_remap);

	// Sort triangles by smallest wedge index
	sort_triangles_by_wedge(mesh);
}

} // namespace nx
