#include "spatial_sort.h"
#include <algorithm>
#include <iostream>
#include <assert.h>
#include "../core/log.h"


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
	//Assumes x, y 0 are in the interval[0, 1] and quantize to 21 bits
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
	std::vector<MortonPair>& temp) {

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
	nx::debug << "Spatial sorting " << num_positions << " positions..." << std::endl;

	if (num_positions == 0) {
		return std::vector<Index>();
	}
	Aabb &bounds = mesh.bounds;
	bounds.max = bounds.min = mesh.positions[0];
	for (Index i = 0; i < num_positions; ++i) {
		const Vector3f& p = mesh.positions[i];
		bounds.max.x = std::max(bounds.max.x, p.x);
		bounds.max.y = std::max(bounds.max.y, p.y);
		bounds.max.z = std::max(bounds.max.z, p.z);
		bounds.min.x = std::min(bounds.min.x, p.x);
		bounds.min.y = std::min(bounds.min.y, p.y);
		bounds.min.z = std::min(bounds.min.z, p.z);
	}

	// Normalize positions to [0, 1]
	float size_x = mesh.bounds.max.x - mesh.bounds.min.x;
	float size_y = mesh.bounds.max.y - mesh.bounds.min.y;
	float size_z = mesh.bounds.max.z - mesh.bounds.min.z;
	float max_dim = std::max({size_x, size_y, size_z});

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

	// Deduplicate vertices that map to the same Morton code.
	// If two vertices have the same code, consider them identical only
	// if their Euclidean distance is within `eps`.
	std::vector<Vector3f> unique_positions;
	unique_positions.reserve(num_positions);

	// eps relative to spatial extent
	float eps = std::max(1e-9f, max_dim * 1e-6f);
	float eps2 = eps * eps;

	Index unique_count = 0;
	for (Index sorted_idx = 0; sorted_idx < num_positions; ++sorted_idx) {
		Index old_idx = pairs[sorted_idx].index;
		const Vector3f& p = mesh.positions[old_idx];

		bool merged = false;
		// If the Morton code is equal to previous entries, search backwards
		// through the sorted list while the code remains equal and try to
		// match against any previously created unique position.
		if (sorted_idx > 0 && pairs[sorted_idx].code == pairs[sorted_idx - 1].code) {
			uint64_t code = pairs[sorted_idx].code;
			Index k = static_cast<int64_t>(sorted_idx) - 1;
			while (k >= 0 && pairs[k].code == code) {
				Index earlier_old = pairs[static_cast<size_t>(k)].index;
				Index mapped = remap[earlier_old];
				
				const Vector3f& cand = unique_positions[mapped];
				if(cand == p) {
					remap[old_idx] = mapped;
					merged = true;
					break;
				}
				--k;
			}
		}

		if (!merged) {
			unique_positions.push_back(p);
			remap[old_idx] = unique_count;
			++unique_count;
		}
	}

	// Write unique positions back (shrink to unique_count)
	nx::debug << "Writing sorted (deduplicated) positions... (" << unique_count << " -> " << num_positions << ")" << std::endl;
	mesh.positions.resize(unique_count);
	for (Index i = 0; i < unique_count; ++i) {
		mesh.positions[i] = unique_positions[i];
	}

	nx::debug << "Spatial sort complete." << std::endl;
	return remap;
}

void remap_wedge_positions(MeshFiles& mesh, const std::vector<Index>& remap) {
	if (remap.empty()) return;

	nx::debug << "Remapping wedge positions..." << std::endl;

	Index num_wedges = mesh.wedges.size();
	for (Index i = 0; i < num_wedges; ++i) {
		Wedge& w = mesh.wedges[i];
		if (w.p < remap.size()) {
			w.p = remap[w.p];
		}
	}

	nx::debug << "Remapping complete." << std::endl;
}

std::vector<Index> sort_wedges_by_position(MeshFiles& mesh) {
	Index num_wedges = mesh.wedges.size();
	nx::debug << "Sorting " << num_wedges << " wedges by position index..." << std::endl;

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

	nx::debug << "Wedge sorting complete." << std::endl;
	return remap;
}

static std::vector<Index> unify_wedges_by_position_and_attributes(
	MeshFiles& mesh,
	float normal_eps,
	float tex_eps) {
	Index num_wedges = mesh.wedges.size();

	nx::debug << "Unifying wedges by position/attributes..." << std::endl;

	const float normal_eps2 = normal_eps * normal_eps;
	const float tex_eps2 = tex_eps * tex_eps;

	std::vector<Index> remap(num_wedges);
	std::vector<Wedge> unified;
	unified.reserve(num_wedges);

	Index start = 0;
	while (start < num_wedges) {
		Index p = mesh.wedges[start].p;
		Index end = start + 1;
		while (end < num_wedges && mesh.wedges[end].p == p) {
			++end;
		}

		Index group_base = static_cast<Index>(unified.size());
		for (Index i = start; i < end; ++i) {
			const Wedge& w = mesh.wedges[i];
			bool merged = false;

			for (Index j = group_base; j < unified.size(); ++j) {
				const Wedge& u = unified[j];
				float ndx = w.n.x - u.n.x;
				float ndy = w.n.y - u.n.y;
				float ndz = w.n.z - u.n.z;
				float td0 = w.t.u - u.t.u;
				float td1 = w.t.v - u.t.v;
				float n2 = ndx * ndx + ndy * ndy + ndz * ndz;
				float t2 = td0 * td0 + td1 * td1;
				if (n2 <= normal_eps2 && t2 <= tex_eps2) {
					remap[i] = j;
					merged = true;
					break;
				}
			}

			if (!merged) {
				remap[i] = static_cast<Index>(unified.size());
				unified.push_back(w);
			}
		}

		start = end;
	}

	mesh.wedges.resize(unified.size());
	for(size_t i = 0; i < unified.size(); i++)
		mesh.wedges[i] = unified[i];

	nx::debug << "Wedge unification complete: " << num_wedges << " -> " << mesh.wedges.size() << std::endl;
	return remap;
}

void remap_triangle_wedges(MeshFiles& mesh, const std::vector<Index>& remap) {
	nx::debug << "Remapping triangle wedge indices..." << std::endl;

	Index num_triangles = mesh.triangles.size();
	for (Index i = 0; i < num_triangles; ++i) {
		Triangle& tri = mesh.triangles[i];
		for (int j = 0; j < 3; ++j) {
				tri.w[j] = remap[tri.w[j]];
		}
	}
}

void sort_triangles_by_wedge(MeshFiles& mesh) {
	Index num_triangles = mesh.triangles.size();
	nx::debug << "Sorting " << num_triangles << " triangles by smallest wedge index..." << std::endl;

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

	nx::debug << "Triangle sorting complete." << std::endl;
}

void spatial_sort_mesh(MeshFiles& mesh) {
	// Sort the vertices spatially and reindex
	std::vector<Index> position_remap = spatial_sort_positions(mesh);
	remap_wedge_positions(mesh, position_remap);
	
	// Sort wedges by position index and reindex triangles
	std::vector<Index> wedge_sort_remap = sort_wedges_by_position(mesh);
	remap_triangle_wedges(mesh, wedge_sort_remap);

	std::vector<Index> wedge_unify_remap = unify_wedges_by_position_and_attributes(mesh, 1e-3f, 1e-5f);
	remap_triangle_wedges(mesh, wedge_unify_remap);

	// Sort triangles by smallest wedge index
	sort_triangles_by_wedge(mesh);
}

} // namespace nx
