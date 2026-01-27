#include "merge_simplify_split.h"

#include <iostream>
#include <unordered_set>
#include <algorithm>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <cstdint>
#include <cmath>
#include <map>

#include <metis.h>

#include "../core/mesh.h"
#include "../core/adjacency.h"
#include "../core/micro_clustering_metis.h"
#include "../core/vcgmesh.h"
#include "../loaders/objexporter.h"

namespace nx {

MergedMesh merge_micronode_clusters(
	const MeshFiles& mesh,
	const MicroNode& micronode) {

	MergedMesh merged;
	merged.parent_micronode_id = micronode.id;

	std::unordered_set<Index> micronode_clusters;
	micronode_clusters.reserve(micronode.cluster_ids.size());
	for (Index cluster_id : micronode.cluster_ids) {
		micronode_clusters.insert(cluster_id);
	}

	std::unordered_map<Index, Index> wedge_map;

	std::size_t total_triangles = 0;
	for (Index cluster_id : micronode.cluster_ids) {
		const Cluster& cluster = mesh.clusters[cluster_id];
		total_triangles += cluster.triangle_count;
	}
	merged.triangles.reserve(total_triangles);

	for (Index cluster_id : micronode.cluster_ids) {
		const Cluster& cluster = mesh.clusters[cluster_id];
		Index start = cluster.triangle_offset;
		Index end = cluster.triangle_offset + cluster.triangle_count;

		for (Index t = start; t < end; ++t) {
			const Triangle& tri = mesh.triangles[t];
			Triangle new_tri{};

			for (int j = 0; j < 3; ++j) {
				Index original_wedge = tri.w[j];
				auto wedge_it = wedge_map.find(original_wedge);
				if (wedge_it == wedge_map.end()) {
					const Wedge& w = mesh.wedges[original_wedge];
					Index original_pos = w.p;

					Index new_pos = 0;
					auto pos_it = merged.position_map.find(original_pos);
					if (pos_it == merged.position_map.end()) {
						new_pos = static_cast<Index>(merged.positions.size());
						merged.positions.push_back(mesh.positions[original_pos]);
						merged.position_map[original_pos] = new_pos;
					} else {
						new_pos = pos_it->second;
					}

					Wedge new_wedge = w;
					new_wedge.p = new_pos;
					Index new_wedge_index = static_cast<Index>(merged.wedges.size());
					merged.wedges.push_back(new_wedge);
					wedge_map[original_wedge] = new_wedge_index;
					new_tri.w[j] = new_wedge_index;
				} else {
					new_tri.w[j] = wedge_it->second;
				}
			}

			merged.triangles.push_back(new_tri);

			assert(mesh.triangle_to_cluster[t] == cluster_id);

			const FaceAdjacency& adj = mesh.adjacency[t];
			for (int corner = 0; corner < 3; ++corner) {
				Index neighbor = adj.opp[corner];
				if (neighbor == cluster_id || neighbor == std::numeric_limits<Index>::max()) {
					continue;
				}
				Index neighbor_cluster = mesh.triangle_to_cluster[neighbor];
				if (micronode_clusters.find(neighbor_cluster) != micronode_clusters.end()) {
					continue;
				}
				Index p0 = mesh.wedges[tri.w[corner]].p;
				Index p1 = mesh.wedges[tri.w[(corner + 1) % 3]].p;

				auto it0 = merged.position_map.find(p0);
				if (it0 != merged.position_map.end()) {
					merged.boundary_vertices.insert(it0->second);
				}
				auto it1 = merged.position_map.find(p1);
				if (it1 != merged.position_map.end()) {
					merged.boundary_vertices.insert(it1->second);
				}
			}
		}
	}

	return merged;
}

void simplify_mesh(
	MergedMesh& mesh,
	Index target_triangle_count) {

	if (mesh.triangles.size() <= target_triangle_count || target_triangle_count == 0) {
		return;
	}

	struct PositionKey {
		std::uint32_t x;
		std::uint32_t y;
		std::uint32_t z;
		bool operator==(const PositionKey& other) const {
			return x == other.x && y == other.y && z == other.z;
		}
	};

	struct PositionKeyHash {
		std::size_t operator()(const PositionKey& key) const {
			std::size_t h = static_cast<std::size_t>(key.x);
			h ^= static_cast<std::size_t>(key.y) + 0x9e3779b9 + (h << 6) + (h >> 2);
			h ^= static_cast<std::size_t>(key.z) + 0x9e3779b9 + (h << 6) + (h >> 2);
			return h;
		}
	};

	auto to_key = [](const Vector3f& p) -> PositionKey {
		PositionKey key;
		std::memcpy(&key.x, &p.x, sizeof(std::uint32_t));
		std::memcpy(&key.y, &p.y, sizeof(std::uint32_t));
		std::memcpy(&key.z, &p.z, sizeof(std::uint32_t));
		return key;
	};

	std::unordered_set<PositionKey, PositionKeyHash> boundary_positions;
	boundary_positions.reserve(mesh.boundary_vertices.size());
	for (Index v : mesh.boundary_vertices) {
		boundary_positions.insert(to_key(mesh.positions[v]));
	}

	VcgMesh legacy_mesh;
	vcg::tri::Allocator<VcgMesh>::AddVertices(legacy_mesh, mesh.triangles.size() * 3);
	vcg::tri::Allocator<VcgMesh>::AddFaces(legacy_mesh, mesh.triangles.size());

	for (std::size_t i = 0; i < mesh.triangles.size(); ++i) {
		const Triangle& tri = mesh.triangles[i];
		AFace& face = legacy_mesh.face[i];
		for (int k = 0; k < 3; ++k) {
			const Wedge& w = mesh.wedges[tri.w[k]];
			const Vector3f& p = mesh.positions[w.p];
			AVertex& v = legacy_mesh.vert[i * 3 + k];
			v.P() = vcg::Point3f(p.x, p.y, p.z);
			v.C() = vcg::Color4b(255, 255, 255, 255);
			face.V(k) = &v;
			face.WN(k) = vcg::Point3f(w.n.x, w.n.y, w.n.z);
			face.WT(k).U() = w.t.u;
			face.WT(k).V() = w.t.v;
		}
		face.node = 0;
	}

	// Lock boundary vertices directly (do not mark faces)
	for (auto& v : legacy_mesh.vert) {
		v.SetW();
		Vector3f pos{v.cP()[0], v.cP()[1], v.cP()[2]};
		if (boundary_positions.count(to_key(pos)) > 0) {
			v.ClearW();
		}
	}

	legacy_mesh.lockVertices();
	legacy_mesh.quadricInit();
	std::uint16_t target_faces = static_cast<std::uint16_t>(
		std::min<std::size_t>(target_triangle_count, static_cast<std::size_t>(std::numeric_limits<std::uint16_t>::max())));
	legacy_mesh.simplify(target_faces, VcgMesh::QUADRICS);

	std::vector<Vector3f> old_positions = mesh.positions;
	std::vector<uint8_t> old_boundary(old_positions.size(), 0);
	for (Index v : mesh.boundary_vertices) {
		if (v < old_boundary.size()) {
			old_boundary[v] = 1;
		}
	}

	std::vector<Index> vertex_remap(legacy_mesh.vert.size(), std::numeric_limits<Index>::max());
	std::vector<uint8_t> old_used(old_positions.size(), 0);
	mesh.boundary_vertices.clear();

	for (std::size_t i = 0; i < legacy_mesh.vert.size(); ++i) {
		AVertex& v = legacy_mesh.vert[i];
		if (v.IsD()) {
			continue;
		}
		Vector3f new_pos;
		new_pos.x = v.P()[0];
		new_pos.y = v.P()[1];
		new_pos.z = v.P()[2];

		Index best_idx = std::numeric_limits<Index>::max();
		float best_dist = std::numeric_limits<float>::max();
		for (Index old_idx = 0; old_idx < old_positions.size(); ++old_idx) {
			if (old_used[old_idx]) {
				continue;
			}
			const Vector3f& old_pos = old_positions[old_idx];
			float dx = old_pos.x - new_pos.x;
			float dy = old_pos.y - new_pos.y;
			float dz = old_pos.z - new_pos.z;
			float dist = dx * dx + dy * dy + dz * dz;
			if (dist < best_dist) {
				best_dist = dist;
				best_idx = old_idx;
			}
		}

		if (best_idx == std::numeric_limits<Index>::max()) {
			continue;
		}

		old_used[best_idx] = 1;
		vertex_remap[i] = best_idx;
		mesh.positions[best_idx] = new_pos;
		if (old_boundary[best_idx] || boundary_positions.count(to_key(new_pos)) > 0) {
			mesh.boundary_vertices.insert(best_idx);
		}
	}

	// NOTE: We keep mesh.position_map intact. It maps original mesh positions
	// to merged mesh positions, which we need to update the original mesh.

	mesh.wedges.clear();
	mesh.triangles.clear();

	const AVertex* vbase = legacy_mesh.vert.empty() ? nullptr : &legacy_mesh.vert[0];
	for (std::size_t i = 0; i < legacy_mesh.face.size(); ++i) {
		AFace& f = legacy_mesh.face[i];
		if (f.IsD()) {
			continue;
		}
		Triangle tri{};
		for (int k = 0; k < 3; ++k) {
			const AVertex* v = f.cV(k);
			std::size_t vidx = static_cast<std::size_t>(v - vbase);
			Index new_pos = vertex_remap[vidx];
			if (new_pos == std::numeric_limits<Index>::max()) {
				continue;
			}
			Wedge w;
			w.p = new_pos;
			const vcg::Point3f& wn = f.WN(k);
			w.n.x = wn[0];
			w.n.y = wn[1];
			w.n.z = wn[2];
			w.t.u = f.WT(k).U();
			w.t.v = f.WT(k).V();
			Index wedge_index = static_cast<Index>(mesh.wedges.size());
			mesh.wedges.push_back(w);
			tri.w[k] = wedge_index;
		}
		mesh.triangles.push_back(tri);
	}

	std::cout << "  [simplify_mesh] Simplified to "
			  << mesh.triangles.size() << " triangles" << std::endl;
}

void simplify_mesh_clustered(
	MergedMesh& mesh,
	Index target_triangle_count) {
	if (mesh.triangles.size() <= target_triangle_count || target_triangle_count == 0) {
		return;
	}


	std::vector<uint8_t> boundary_flag(mesh.positions.size(), 0);
	for (Index v : mesh.boundary_vertices) {
		boundary_flag[v] = 1;
	}

	// Compute bounding box of positions
	Vector3f min_pt{ std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max() };
	Vector3f max_pt{ std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest() };
	for (const Vector3f& p : mesh.positions) {
		min_pt.x = std::min(min_pt.x, p.x);
		min_pt.y = std::min(min_pt.y, p.y);
		min_pt.z = std::min(min_pt.z, p.z);
		max_pt.x = std::max(max_pt.x, p.x);
		max_pt.y = std::max(max_pt.y, p.y);
		max_pt.z = std::max(max_pt.z, p.z);
	}

	// Estimate average edge length
	float total_edge_len = 0.0f;
	std::size_t edge_count = 0;
	for (const Triangle& tri : mesh.triangles) {
		Index p0 = mesh.wedges[tri.w[0]].p;
		Index p1 = mesh.wedges[tri.w[1]].p;
		Index p2 = mesh.wedges[tri.w[2]].p;
		const Vector3f& a = mesh.positions[p0];
		const Vector3f& b = mesh.positions[p1];
		const Vector3f& c = mesh.positions[p2];
		float dx01 = a.x - b.x; float dy01 = a.y - b.y; float dz01 = a.z - b.z;
		float dx12 = b.x - c.x; float dy12 = b.y - c.y; float dz12 = b.z - c.z;
		float dx20 = c.x - a.x; float dy20 = c.y - a.y; float dz20 = c.z - a.z;
		total_edge_len += std::sqrt(dx01*dx01 + dy01*dy01 + dz01*dz01);
		total_edge_len += std::sqrt(dx12*dx12 + dy12*dy12 + dz12*dz12);
		total_edge_len += std::sqrt(dx20*dx20 + dy20*dy20 + dz20*dz20);
		edge_count += 3;
	}
	float avg_edge_len = (edge_count > 0) ? (total_edge_len / static_cast<float>(edge_count)) : 0.0f;
	if (avg_edge_len <= 0.0f) {
		// Degenerate; just take a prefix
		mesh.triangles.resize(target_triangle_count);
		return;
	}

	// Use average edge length as clustering cell size
	float cell_size = avg_edge_len*1.41;

	auto cell_key = [&](const Vector3f& p) -> std::uint64_t {
		std::uint32_t ix = static_cast<std::uint32_t>((p.x - min_pt.x) / cell_size);
		std::uint32_t iy = static_cast<std::uint32_t>((p.y - min_pt.y) / cell_size);
		std::uint32_t iz = static_cast<std::uint32_t>((p.z - min_pt.z) / cell_size);
		return (static_cast<std::uint64_t>(ix) << 42) |
			   (static_cast<std::uint64_t>(iy) << 21) |
			   (static_cast<std::uint64_t>(iz));
	};


	struct ClusterAccum {
		Vector3f sum{0.0f, 0.0f, 0.0f};
		Index count = 0;
		Index rep = std::numeric_limits<Index>::max();
	};

	std::unordered_map<std::uint64_t, ClusterAccum> clusters;
	clusters.reserve(mesh.positions.size());
	std::vector<Index> pos_to_rep(mesh.positions.size(), std::numeric_limits<Index>::max());

	for (Index i = 0; i < mesh.positions.size(); ++i) {
		const Vector3f& p = mesh.positions[i];
		if (boundary_flag[i]) {
			pos_to_rep[i] = i;
			clusters[i].rep = i;
			clusters[i].sum = p;
			clusters[i].count = 1;
			continue;
		}
		std::uint64_t key = cell_key(p);
		auto& acc = clusters[key];
		if (acc.rep == std::numeric_limits<Index>::max()) {
			acc.rep = i;
		}
		acc.sum.x += p.x;
		acc.sum.y += p.y;
		acc.sum.z += p.z;
		acc.count += 1;
		pos_to_rep[i] = acc.rep;
	}

	// Update representative positions to cluster centers
	for (auto& [key, acc] : clusters) {
		if (boundary_flag[acc.rep]) {
			continue;
		}
		Vector3f center;
		center.x = acc.sum.x / static_cast<float>(acc.count);
		center.y = acc.sum.y / static_cast<float>(acc.count);
		center.z = acc.sum.z / static_cast<float>(acc.count);
		mesh.positions[acc.rep] = center;
	}

	// Remap wedges to representative positions
	for (Wedge& w : mesh.wedges) {
			w.p = pos_to_rep[w.p];
	}

	// Remove degenerate triangles (collapsed after remap)
	std::vector<Triangle> new_tris;
	new_tris.reserve(mesh.triangles.size());
	for (const Triangle& tri : mesh.triangles) {
		Index p0 = mesh.wedges[tri.w[0]].p;
		Index p1 = mesh.wedges[tri.w[1]].p;
		Index p2 = mesh.wedges[tri.w[2]].p;
		if (p0 == p1 || p1 == p2 || p0 == p2) {
			continue;
		}
		new_tris.push_back(tri);
	}
	mesh.triangles.swap(new_tris);

	std::cout << "  [simplify_mesh_clustered] Reduced to "
			  << mesh.triangles.size() << " triangles after clustering positions from" << new_tris.size() << std::endl;

}

void update_vertices_and_wedges(
	MeshFiles& next_mesh,
	const MergedMesh& merged) {
	if (merged.position_map.empty()) {
		return;
	}

	std::unordered_map<Index, const Wedge*> merged_wedge_for_pos;
	merged_wedge_for_pos.reserve(merged.wedges.size());
	for (const Wedge& w : merged.wedges) {
		if (merged_wedge_for_pos.find(w.p) == merged_wedge_for_pos.end()) {
			merged_wedge_for_pos[w.p] = &w;
		}
	}

	for (const auto& [original_pos, merged_pos] : merged.position_map) {
		if (original_pos < next_mesh.positions.size() && merged_pos < merged.positions.size()) {
			next_mesh.positions[original_pos] = merged.positions[merged_pos];
		}
	}

	for (Index i = 0; i < next_mesh.wedges.size(); ++i) {
		Wedge& w = next_mesh.wedges[i];
		auto it = merged.position_map.find(w.p);
		if (it == merged.position_map.end()) {
			continue;
		}
		Index merged_pos = it->second;
		auto mw_it = merged_wedge_for_pos.find(merged_pos);
		if (mw_it == merged_wedge_for_pos.end()) {
			continue;
		}
		const Wedge* mw = mw_it->second;
		w.n = mw->n;
		w.t = mw->t;
	}
}



std::vector<FaceAdjacency> build_adjacency_for_merged(const MergedMesh& mesh) {
	static constexpr Index BORDER = std::numeric_limits<Index>::max();

	Index num_triangles = static_cast<Index>(mesh.triangles.size());
	std::vector<FaceAdjacency> adjacency(num_triangles);
	for (Index i = 0; i < num_triangles; ++i) {
		for (int j = 0; j < 3; ++j) {
			adjacency[i].opp[j] = BORDER;
		}
	}

	std::vector<HalfedgeRecord> halfedges;
	halfedges.reserve(num_triangles * 3);

	for (Index face_idx = 0; face_idx < num_triangles; ++face_idx) {
		const Triangle& tri = mesh.triangles[face_idx];
		Index p[3];
		for (int i = 0; i < 3; ++i) {
			p[i] = mesh.wedges[tri.w[i]].p;
		}
		for (int corner = 0; corner < 3; ++corner) {
			HalfedgeRecord he;
			he.v0 = p[corner];
			he.v1 = p[(corner + 1) % 3];
			he.face = face_idx;
			he.corner = corner;
			halfedges.push_back(he);
		}
	}

	std::sort(halfedges.begin(), halfedges.end(), [](const HalfedgeRecord& a, const HalfedgeRecord& b) {
		Index a_min = std::min(a.v0, a.v1);
		Index a_max = std::max(a.v0, a.v1);
		Index b_min = std::min(b.v0, b.v1);
		Index b_max = std::max(b.v0, b.v1);

		if (a_min != b_min) return a_min < b_min;
		if (a_max != b_max) return a_max < b_max;
		return a.v0 < b.v0;
	});

	size_t i = 0;
	while (i < halfedges.size()) {
		const HalfedgeRecord& he = halfedges[i];
		Index edge_min = std::min(he.v0, he.v1);
		Index edge_max = std::max(he.v0, he.v1);
		size_t j = i;
		while (j < halfedges.size()) {
			const HalfedgeRecord& other = halfedges[j];
			Index other_min = std::min(other.v0, other.v1);
			Index other_max = std::max(other.v0, other.v1);
			if (other_min != edge_min || other_max != edge_max) {
				break;
			}
			j++;
		}

		if (j - i == 2) {
			const HalfedgeRecord& he0 = halfedges[i];
			const HalfedgeRecord& he1 = halfedges[i + 1];
			if ((he0.v0 == he1.v1) && (he0.v1 == he1.v0)) {
				adjacency[he0.face].opp[he0.corner] = he1.face;
				adjacency[he1.face].opp[he1.corner] = he0.face;
			}
		}

		i = j;
	}

	return adjacency;
}

DualPartitionSplit split_triangles_dual_partition_impl(
	const MergedMesh& mesh,
	const std::vector<Index>& triangle_indices,
	const Vector3f& primary_centroid,
	const Vector3f& dual_centroid,
	const std::vector<FaceAdjacency>& adjacency) {

	DualPartitionSplit result;
	if (triangle_indices.size() < 2) {
		result.primary_triangles = triangle_indices;
		return result;
	}

	std::unordered_map<Index, Index> tri_to_local;
	tri_to_local.reserve(triangle_indices.size());
	for (std::size_t i = 0; i < triangle_indices.size(); ++i) {
		tri_to_local[triangle_indices[i]] = static_cast<Index>(i);
	}

	std::size_t num_nodes = triangle_indices.size();
	std::vector<idx_t> xadj;
	std::vector<idx_t> adjncy;
	std::vector<idx_t> adjwgt;
	std::vector<float> dist_primary_list(triangle_indices.size());
	std::vector<float> dist_dual_list(triangle_indices.size());
	std::vector<float> w_primary(triangle_indices.size());
	std::vector<float> w_dual(triangle_indices.size());
	float avg_dist = 0.0f;

	for (std::size_t i = 0; i < triangle_indices.size(); ++i) {
		Index tri_idx = triangle_indices[i];
		const Triangle& tri = mesh.triangles[tri_idx];
		Vector3f tri_center = {0.0f, 0.0f, 0.0f};
		for (int v = 0; v < 3; ++v) {
			const Vector3f& pos = mesh.positions[mesh.wedges[tri.w[v]].p];
			tri_center.x += pos.x;
			tri_center.y += pos.y;
			tri_center.z += pos.z;
		}
		tri_center.x /= 3.0f;
		tri_center.y /= 3.0f;
		tri_center.z /= 3.0f;

		float dx_p = tri_center.x - primary_centroid.x;
		float dy_p = tri_center.y - primary_centroid.y;
		float dz_p = tri_center.z - primary_centroid.z;
		float dist_primary = std::sqrt(dx_p*dx_p + dy_p*dy_p + dz_p*dz_p);

		float dx_d = tri_center.x - dual_centroid.x;
		float dy_d = tri_center.y - dual_centroid.y;
		float dz_d = tri_center.z - dual_centroid.z;
		float dist_dual = std::sqrt(dx_d*dx_d + dy_d*dy_d + dz_d*dz_d);

		dist_primary_list[i] = dist_primary;
		dist_dual_list[i] = dist_dual;
		avg_dist += 0.5f * (dist_primary + dist_dual);
	}
	avg_dist = avg_dist / static_cast<float>(triangle_indices.size());
	if (avg_dist <= 0.0f) avg_dist = 1.0f;

	for (std::size_t i = 0; i < triangle_indices.size(); ++i) {
		w_primary[i] = avg_dist / (dist_primary_list[i] + avg_dist);
		w_dual[i] = avg_dist / (dist_dual_list[i] + avg_dist);
	}

	xadj.reserve(num_nodes + 1);
	xadj.push_back(0);

	const int base_edge_weight = 10;
	for (std::size_t i = 0; i < triangle_indices.size(); ++i) {
		Index tri_idx = triangle_indices[i];
		const FaceAdjacency& adj = adjacency[tri_idx];

		for (int corner = 0; corner < 3; ++corner) {
			Index neighbor_tri = adj.opp[corner];
			if (neighbor_tri != std::numeric_limits<Index>::max()) {
				auto it = tri_to_local.find(neighbor_tri);
				if (it != tri_to_local.end()) {
					Index j = it->second;
					float affinity = (w_primary[i] * w_primary[j]) + (w_dual[i] * w_dual[j]);
					int w = std::max(1, static_cast<int>(base_edge_weight * affinity));
					adjncy.push_back(static_cast<idx_t>(j));
					adjwgt.push_back(w);
				}
			}
		}
		xadj.push_back(static_cast<idx_t>(adjncy.size()));
	}

	idx_t nvtxs = static_cast<idx_t>(num_nodes);
	idx_t ncon = 1;
	idx_t nparts = 2;
	idx_t objval;
	std::vector<idx_t> part(num_nodes);

	idx_t options[METIS_NOPTIONS];
	METIS_SetDefaultOptions(options);
	options[METIS_OPTION_OBJTYPE] = METIS_OBJTYPE_CUT;
	options[METIS_OPTION_NUMBERING] = 0;
	options[METIS_OPTION_UFACTOR] = 1;

	int ret = METIS_PartGraphKway(
		&nvtxs, &ncon,
		xadj.data(), adjncy.data(),
		nullptr, nullptr, adjwgt.data(),
		&nparts, nullptr, nullptr,
		options, &objval, part.data()
		);

	if (ret != METIS_OK) {
		result.primary_triangles = triangle_indices;
		return result;
	}

	std::size_t count0 = 0;
	std::size_t count1 = 0;
	float sum_p0 = 0.0f;
	float sum_p1 = 0.0f;
	for (std::size_t i = 0; i < triangle_indices.size(); ++i) {
		if (part[i] == 0) { sum_p0 += dist_primary_list[i]; count0++; }
		else { sum_p1 += dist_primary_list[i]; count1++; }
	}

	if (count0 == 0 || count1 == 0) {
		std::vector<std::pair<float, Index>> scored;
		scored.reserve(triangle_indices.size());
		for (std::size_t i = 0; i < triangle_indices.size(); ++i) {
			float delta = dist_primary_list[i] - dist_dual_list[i];
			scored.emplace_back(delta, triangle_indices[i]);
		}
		std::sort(scored.begin(), scored.end(), [](const auto& a, const auto& b) {
			return a.first < b.first;
		});
		std::size_t half = scored.size() / 2;
		for (std::size_t i = 0; i < scored.size(); ++i) {
			if (i < half) result.primary_triangles.push_back(scored[i].second);
			else result.dual_triangles.push_back(scored[i].second);
		}
		return result;
	}

	float mean_p0 = sum_p0 / static_cast<float>(count0);
	float mean_p1 = sum_p1 / static_cast<float>(count1);
	idx_t primary_part = (mean_p0 <= mean_p1) ? 0 : 1;
	idx_t dual_part = (primary_part == 0) ? 1 : 0;

	for (std::size_t i = 0; i < triangle_indices.size(); ++i) {
		Index tri_idx = triangle_indices[i];
		if (part[i] == primary_part) {
			result.primary_triangles.push_back(tri_idx);
		} else if (part[i] == dual_part) {
			result.dual_triangles.push_back(tri_idx);
		} else {
			result.primary_triangles.push_back(tri_idx);
		}
	}

	return result;
}

void compute_cluster_bounds(
	const MeshFiles& mesh,
	Index triangle_offset,
	Index triangle_count,
	Vector3f& center,
	float& radius) {
	if (triangle_count == 0) {
		center = {0.0f, 0.0f, 0.0f};
		radius = 0.0f;
		return;
	}

	center = {0.0f, 0.0f, 0.0f};
	float inv_count = 1.0f / (static_cast<float>(triangle_count) * 3.0f);
	for (Index t = triangle_offset; t < triangle_offset + triangle_count; ++t) {
		const Triangle& tri = mesh.triangles[t];
		for (int k = 0; k < 3; ++k) {
			const Vector3f& p = mesh.positions[mesh.wedges[tri.w[k]].p];
			center.x += p.x * inv_count;
			center.y += p.y * inv_count;
			center.z += p.z * inv_count;
		}
	}

	radius = 0.0f;
	for (Index t = triangle_offset; t < triangle_offset + triangle_count; ++t) {
		const Triangle& tri = mesh.triangles[t];
		for (int k = 0; k < 3; ++k) {
			const Vector3f& p = mesh.positions[mesh.wedges[tri.w[k]].p];
			float dx = p.x - center.x;
			float dy = p.y - center.y;
			float dz = p.z - center.z;
			float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
			radius = std::max(radius, dist);
		}
	}
}

Index ensure_mapped_position(Index merged_pos, const std::vector<Index>& merged_to_original) {
	if (merged_pos < merged_to_original.size()) {
		return merged_to_original[merged_pos];
	}
	return std::numeric_limits<Index>::max();
}

DualPartitionSplit split_triangles_dual_partition(
	const MergedMesh& mesh,
	const std::vector<Index>& triangle_indices,
	const Vector3f& primary_centroid,
	const Vector3f& dual_centroid) {
	std::vector<FaceAdjacency> adjacency = build_adjacency_for_merged(mesh);
	return split_triangles_dual_partition_impl(mesh, triangle_indices, primary_centroid, dual_centroid, adjacency);
}


void split_mesh(
	const MergedMesh& merged,
	const MicroNode& micronode,
	MeshFiles& next_mesh) {

	// Find the two parent micronodes in next_mesh containing this micronode as child
	std::vector<Index> parent_indices;
	for (Index i = 0; i < next_mesh.micronodes.size(); ++i) {
		const MicroNode& parent = next_mesh.micronodes[i];
		if (std::find(parent.children_nodes.begin(), parent.children_nodes.end(), micronode.id) != parent.children_nodes.end()) {
			parent_indices.push_back(i);
		}
	}

	if (parent_indices.size() != 2) {
		throw std::runtime_error("Expected exactly two parent micronodes for split_mesh.");
	}

	MicroNode& parent_primary = next_mesh.micronodes[parent_indices[0]];
	MicroNode& parent_dual = next_mesh.micronodes[parent_indices[1]];

	std::vector<Index> triangle_indices;
	triangle_indices.reserve(merged.triangles.size());
	for (Index i = 0; i < merged.triangles.size(); ++i) {
		triangle_indices.push_back(i);
	}

	DualPartitionSplit split = split_triangles_dual_partition(
		merged,
		triangle_indices,
		parent_primary.centroid,
		parent_dual.centroid);

	// Build merged_pos -> original_pos mapping
	std::vector<Index> merged_to_original(merged.positions.size(), std::numeric_limits<Index>::max());
	for (const auto& [original_pos, merged_pos] : merged.position_map) {
		assert(merged_pos < merged_to_original.size());
		merged_to_original[merged_pos] = original_pos;
	}

	auto append_cluster = [&](const std::vector<Index>& tri_indices, MicroNode& parent_node) {
		if (tri_indices.empty()) {
			return;
		}

		Index cluster_id = static_cast<Index>(next_mesh.clusters.size());
		Index triangle_offset = static_cast<Index>(next_mesh.triangles.size());
		Index wedge_offset = static_cast<Index>(next_mesh.wedges.size());
		Index triangle_count = static_cast<Index>(tri_indices.size());
		Index wedge_count = triangle_count * 3;

		next_mesh.triangles.resize(triangle_offset + triangle_count);
		next_mesh.wedges.resize(wedge_offset + wedge_count);

		Index tri_local = 0;
		Index wedge_local = 0;
		for (Index tri_idx : tri_indices) {
			const Triangle& tri = merged.triangles[tri_idx];
			Triangle new_tri{};
			for (int k = 0; k < 3; ++k) {
				const Wedge& mw = merged.wedges[tri.w[k]];
				Index original_pos = ensure_mapped_position(mw.p, merged_to_original);
				if (original_pos == std::numeric_limits<Index>::max()) {
					throw std::runtime_error("split_mesh: missing merged->original position mapping");
				}
				Wedge new_wedge = mw;
				new_wedge.p = original_pos;
				Index new_wedge_index = wedge_offset + wedge_local;
				next_mesh.wedges[new_wedge_index] = new_wedge;
				new_tri.w[k] = new_wedge_index;
				wedge_local++;
			}
			next_mesh.triangles[triangle_offset + tri_local] = new_tri;
			tri_local++;
		}
		Cluster cluster{};
		cluster.triangle_offset = triangle_offset;
		cluster.triangle_count = triangle_count;
		compute_cluster_bounds(next_mesh, triangle_offset, triangle_count, cluster.center, cluster.radius);
		Index new_cluster_index = static_cast<Index>(next_mesh.clusters.size());
		next_mesh.clusters.resize(new_cluster_index + 1);
		next_mesh.clusters[new_cluster_index] = cluster;

		if (next_mesh.triangle_to_cluster.size() < next_mesh.triangles.size()) {
			next_mesh.triangle_to_cluster.resize(next_mesh.triangles.size());
		}
		for (Index t = triangle_offset; t < triangle_offset + triangle_count; ++t) {
			next_mesh.triangle_to_cluster[t] = cluster_id;
		}

		parent_node.cluster_ids.push_back(cluster_id);
		parent_node.triangle_count += triangle_count;
		std::cout << "Cluster size: " << cluster.triangle_count << std::endl;
	};
	append_cluster(split.primary_triangles, parent_primary);
	append_cluster(split.dual_triangles, parent_dual);

}

std::vector<Index> split_simplified_micronode(
	const MergedMesh& merged,
	MicroNode& micronode,
	MeshFiles& next_mesh,
	std::size_t max_triangles) {

	std::vector<Index> created_clusters;

	if (merged.triangles.empty()) {
		return created_clusters;
	}

	// Build merged_pos -> original_pos mapping
	std::vector<Index> merged_to_original(merged.positions.size(), std::numeric_limits<Index>::max());
	for (const auto& [original_pos, merged_pos] : merged.position_map) {
		assert(merged_pos < merged_to_original.size());
		merged_to_original[merged_pos] = original_pos;
	}

	std::size_t num_triangles = merged.triangles.size();

	// Determine number of clusters to create
	std::size_t num_clusters = (num_triangles + max_triangles - 1) / max_triangles;
	if (num_clusters < 1) num_clusters = 1;
	if (num_clusters > num_triangles) num_clusters = num_triangles;

	std::vector<idx_t> part(num_triangles, 0);

	if (num_clusters > 1) {
		// Build adjacency for METIS
		std::vector<FaceAdjacency> adjacency = build_adjacency_for_merged(merged);

		std::vector<idx_t> xadj;
		std::vector<idx_t> adjncy;
		std::vector<idx_t> adjwgt;

		xadj.reserve(num_triangles + 1);
		xadj.push_back(0);

		for (std::size_t i = 0; i < num_triangles; ++i) {
			const FaceAdjacency& adj = adjacency[i];
			for (int corner = 0; corner < 3; ++corner) {
				Index neighbor = adj.opp[corner];
				if (neighbor != std::numeric_limits<Index>::max()) {
					adjncy.push_back(static_cast<idx_t>(neighbor));
					adjwgt.push_back(1);
				}
			}
			xadj.push_back(static_cast<idx_t>(adjncy.size()));
		}

		if (!adjncy.empty()) {
			idx_t nvtxs = static_cast<idx_t>(num_triangles);
			idx_t ncon = 1;
			idx_t nparts = static_cast<idx_t>(num_clusters);
			idx_t objval;

			idx_t options[METIS_NOPTIONS];
			METIS_SetDefaultOptions(options);
			options[METIS_OPTION_OBJTYPE] = METIS_OBJTYPE_CUT;
			options[METIS_OPTION_NUMBERING] = 0;

			int ret = METIS_PartGraphKway(
				&nvtxs, &ncon,
				xadj.data(), adjncy.data(),
				nullptr, nullptr, adjwgt.data(),
				&nparts, nullptr, nullptr,
				options, &objval, part.data());

			if (ret != METIS_OK) {
				// Fallback: sequential assignment
				for (std::size_t i = 0; i < num_triangles; ++i) {
					part[i] = static_cast<idx_t>(i * num_clusters / num_triangles);
				}
			}
		} else {
			// No adjacency, sequential assignment
			for (std::size_t i = 0; i < num_triangles; ++i) {
				part[i] = static_cast<idx_t>(i * num_clusters / num_triangles);
			}
		}
	}

	// Find actual number of partitions used
	idx_t max_part = 0;
	for (std::size_t i = 0; i < num_triangles; ++i) {
		max_part = std::max(max_part, part[i]);
	}
	std::size_t actual_clusters = static_cast<std::size_t>(max_part) + 1;

	// Group triangles by partition
	std::vector<std::vector<Index>> cluster_triangles(actual_clusters);
	for (std::size_t i = 0; i < num_triangles; ++i) {
		cluster_triangles[part[i]].push_back(static_cast<Index>(i));
	}

	// Create clusters in next_mesh
	for (std::size_t c = 0; c < actual_clusters; ++c) {
		const std::vector<Index>& tri_indices = cluster_triangles[c];
		if (tri_indices.empty()) continue;

		Index cluster_id = static_cast<Index>(next_mesh.clusters.size());
		Index triangle_offset = static_cast<Index>(next_mesh.triangles.size());
		Index wedge_offset = static_cast<Index>(next_mesh.wedges.size());
		Index triangle_count = static_cast<Index>(tri_indices.size());
		Index wedge_count = triangle_count * 3;

		next_mesh.triangles.resize(triangle_offset + triangle_count);
		next_mesh.wedges.resize(wedge_offset + wedge_count);

		Index tri_local = 0;
		Index wedge_local = 0;
		for (Index tri_idx : tri_indices) {
			const Triangle& tri = merged.triangles[tri_idx];
			Triangle new_tri{};
			for (int k = 0; k < 3; ++k) {
				const Wedge& mw = merged.wedges[tri.w[k]];
				Index original_pos = ensure_mapped_position(mw.p, merged_to_original);
				if (original_pos == std::numeric_limits<Index>::max()) {
					throw std::runtime_error("split_simplified_micronode: missing merged->original position mapping");
				}
				Wedge new_wedge = mw;
				new_wedge.p = original_pos;
				Index new_wedge_index = wedge_offset + wedge_local;
				next_mesh.wedges[new_wedge_index] = new_wedge;
				new_tri.w[k] = new_wedge_index;
				wedge_local++;
			}
			next_mesh.triangles[triangle_offset + tri_local] = new_tri;
			tri_local++;
		}

		Cluster cluster{};
		cluster.triangle_offset = triangle_offset;
		cluster.triangle_count = triangle_count;
		compute_cluster_bounds(next_mesh, triangle_offset, triangle_count, cluster.center, cluster.radius);

		next_mesh.clusters.resize(cluster_id + 1);
		next_mesh.clusters[cluster_id] = cluster;

		if (next_mesh.triangle_to_cluster.size() < next_mesh.triangles.size()) {
			next_mesh.triangle_to_cluster.resize(next_mesh.triangles.size());
		}
		for (Index t = triangle_offset; t < triangle_offset + triangle_count; ++t) {
			next_mesh.triangle_to_cluster[t] = cluster_id;
		}

		created_clusters.push_back(cluster_id);
	}

	// Store created cluster indices in micronode.children_nodes (temporary)
	// This will be updated later to point to actual micronode IDs
	micronode.children_nodes.clear();
	for (Index cluster_id : created_clusters) {
		micronode.children_nodes.push_back(cluster_id);
	}

	return created_clusters;
}


} // namespace nx
