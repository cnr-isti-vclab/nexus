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
#include <tuple>

#include <metis.h>

#include "../core/mesh.h"
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

			size_t p[3];
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
						merged.position_remap.push_back(original_pos);
					} else {
						new_pos = pos_it->second;
					}
					Wedge new_wedge;
					new_wedge = w;
					p[j] = new_wedge.p = new_pos;
					Index new_wedge_index = static_cast<Index>(merged.wedges.size());
					merged.wedges.push_back(new_wedge);
					wedge_map[original_wedge] = new_wedge_index;
					new_tri.w[j] = new_wedge_index;
				} else {
					new_tri.w[j] = wedge_it->second;
					p[j] = merged.wedges[wedge_it->second].p;
				}
			}
			//remove degenerate triangles
			if(new_tri.w[0] != new_tri.w[1] && new_tri.w[0] != new_tri.w[2] && new_tri.w[1] != new_tri.w[2] )
				if(p[0] != p[1] && p[0] != p[2] && p[1] != p[2])
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
					merged.boundary_vertices.push_back(it0->second);
				}
				auto it1 = merged.position_map.find(p1);
				if (it1 != merged.position_map.end()) {
					merged.boundary_vertices.push_back(it1->second);
				}
			}
		}
	}
	// Deduplicate boundary vertices
	if (!merged.boundary_vertices.empty()) {
		std::sort(merged.boundary_vertices.begin(), merged.boundary_vertices.end());
		merged.boundary_vertices.erase(std::unique(merged.boundary_vertices.begin(), merged.boundary_vertices.end()),
									   merged.boundary_vertices.end());
	}

	return merged;
}

void simplify_mesh(
	MergedMesh& mesh,
	Index target_triangle_count) {

	if (mesh.triangles.size() <= target_triangle_count || target_triangle_count == 0) {
		return;
	}

	if(0){
		MeshFiles debug_mesh;
		debug_mesh.positions.resize(mesh.positions.size());
		for (Index i = 0; i < mesh.positions.size(); ++i) {
			debug_mesh.positions[i] = mesh.positions[i];
		}
		debug_mesh.wedges.resize(mesh.wedges.size());
		for (Index i = 0; i < mesh.wedges.size(); ++i) {
			debug_mesh.wedges[i] = mesh.wedges[i];
		}
		debug_mesh.triangles.resize(mesh.triangles.size());
		for (Index i = 0; i < mesh.triangles.size(); ++i) {
			debug_mesh.triangles[i] = mesh.triangles[i];
		}
		export_obj(debug_mesh, "debug_before.obj");
	}


	VcgMesh legacy_mesh;
	vcg::tri::Allocator<VcgMesh>::AddVertices(legacy_mesh, mesh.positions.size() * 3);
	vcg::tri::Allocator<VcgMesh>::AddFaces(legacy_mesh, mesh.triangles.size());

	for (std::size_t i = 0; i < mesh.positions.size(); ++i) {
		AVertex &v = legacy_mesh.vert[i];
		const Vector3f& p = mesh.positions[i];
		v.P() = vcg::Point3f(p.x, p.y, p.z);
		v.C() = vcg::Color4b(255, 255, 255, 255);
	}

	// Lock boundary vertices directly
	for(size_t b: mesh.boundary_vertices) {
		legacy_mesh.vert[b].ClearW();
	}

	for (std::size_t i = 0; i < mesh.triangles.size(); ++i) {
		const Triangle& tri = mesh.triangles[i];
		AFace& face = legacy_mesh.face[i];
		for (int k = 0; k < 3; ++k) {
			const Wedge& w = mesh.wedges[tri.w[k]];

			face.V(k) = &legacy_mesh.vert[w.p];

			AVertex* v = face.V(k);
			v->N() = vcg::Point3f(w.n.x, w.n.y, w.n.z);
			//face.WN(k) = vcg::Point3f(w.n.x, w.n.y, w.n.z);
			face.WT(k).U() = w.t.u;
			face.WT(k).V() = w.t.v;
		}
	}
	//vcg::tri::UpdateNormal<VcgMesh>::PerWedgeCrease(legacy_mesh, 60*M_PI/180);
	vcg::tri::UpdateNormal<VcgMesh>::PerVertex(legacy_mesh);
	legacy_mesh.quadricInit();
	legacy_mesh.simplify(target_triangle_count, VcgMesh::QUADRICS);

	//Compact positions and keep track of remapping
	std::vector<Index> vertex_remap(mesh.positions.size(), std::numeric_limits<Index>::max());

	for(size_t i = 0; i < legacy_mesh.vert.size(); i++) {
		AVertex &v = legacy_mesh.vert[i];
		v.SetD();
	}
	const AVertex* vbase = &legacy_mesh.vert[0];
	for (std::size_t i = 0; i < legacy_mesh.face.size(); ++i) {
		AFace& f = legacy_mesh.face[i];
		if (f.IsD()) {
			continue;
		}
		for (int k = 0; k < 3; ++k) {
			AVertex* v = f.V(k);
			v->ClearD();
		}
	}

	size_t count = 0;
	for(size_t i = 0; i < legacy_mesh.vert.size(); i++) {
		AVertex &v = legacy_mesh.vert[i];
		if(v.IsD()) {
			assert(v.IsW());
			continue;
		}
		vcg::Point3f &p = v.P();
		vertex_remap[i] = count;
		mesh.position_remap[count] = mesh.position_remap[i];
		mesh.positions[count++] = { p[0], p[1], p[2]};
	}
	mesh.positions.resize(count);

	//remap boundary vertices (for no good reason).
	for(Index &i: mesh.boundary_vertices)
		i = vertex_remap[i];

	// Remove invalid and duplicate boundary indices
	if (!mesh.boundary_vertices.empty()) {
		// erase invalid markers (could be max value)
		mesh.boundary_vertices.erase(
			std::remove_if(mesh.boundary_vertices.begin(), mesh.boundary_vertices.end(),
						   [](Index v){ return v == std::numeric_limits<Index>::max(); }),
			mesh.boundary_vertices.end());

		if (!mesh.boundary_vertices.empty()) {
			std::sort(mesh.boundary_vertices.begin(), mesh.boundary_vertices.end());
			mesh.boundary_vertices.erase(std::unique(mesh.boundary_vertices.begin(), mesh.boundary_vertices.end()),
										 mesh.boundary_vertices.end());
		}
	}


	mesh.wedges.clear();
	mesh.triangles.clear();

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
			assert(new_pos != std::numeric_limits<Index>::max());

			Wedge w;
			w.p = new_pos;
			w.n.x = v->N()[0];
			w.n.y = v->N()[1];
			w.n.z = v->N()[2];
			w.t.u = f.WT(k).U();
			w.t.v = f.WT(k).V();
			w.t.u = w.t.v = 0;
			Index wedge_index = static_cast<Index>(mesh.wedges.size());
			mesh.wedges.push_back(w);
			tri.w[k] = wedge_index;

			/* DEBUG CHECK for dragon.ply only
			if(k > 1) {
				const AVertex* ov = f.cV(k-1);
				std::size_t ovidx = static_cast<std::size_t>(ov - vbase);
				Index old_pos = vertex_remap[ovidx];
				Vector3f p = mesh.positions[new_pos];
				Vector3f op = mesh.positions[old_pos];
				if(fabs(p.x- op.x) > 0.05 || fabs(p.y - op.y) > 0.05 || fabs(p.z - op.z) > 0.05)
					throw "porcatrottola";
			}
*/
		}
		mesh.triangles.push_back(tri);
	}

	// Compact wedges: sort by (position, normal, texcoord) and merge duplicates
	{
		const Index old_count = static_cast<Index>(mesh.wedges.size());
		using Key = std::tuple<Index, float, float, float, float, float>;
		std::vector<std::pair<Key, Index>> entries;
		entries.reserve(old_count);

		for (Index wi = 0; wi < old_count; ++wi) {
			const Wedge& w = mesh.wedges[wi];
			Key k = std::make_tuple(w.p, w.n.x, w.n.y, w.n.z, w.t.u, w.t.v);
			entries.emplace_back(k, wi);
		}

		std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) {
			return a.first < b.first;
		});

		std::vector<Index> remap(old_count, std::numeric_limits<Index>::max());
		std::vector<Wedge> compacted;
		compacted.reserve(entries.size());

		Key prev_key;
		bool has_prev = false;
		for (const auto& ent : entries) {
			const Key& key = ent.first;
			Index old_idx = ent.second;
			if (!has_prev || key != prev_key) {
				// create new compacted wedge from old
				compacted.push_back(mesh.wedges[old_idx]);
				Index new_idx = static_cast<Index>(compacted.size()) - 1;
				remap[old_idx] = new_idx;
				prev_key = key;
				has_prev = true;
			} else {
				// reuse last compacted index
				remap[old_idx] = static_cast<Index>(compacted.size()) - 1;
			}
		}

		// For any entries that were duplicates but not contiguous (shouldn't happen), ensure remap filled:
		// (but above handles all duplicates because entries contains all wedges)

		// Replace wedges with compacted list
		mesh.wedges = std::move(compacted);

		// Update triangle wedge indices
		for (Triangle& tri : mesh.triangles) {
			for (int k = 0; k < 3; ++k) {
				Index old_w = tri.w[k];
				if (old_w < remap.size()) {
					tri.w[k] = remap[old_w];
				} else {
					tri.w[k] = std::numeric_limits<Index>::max();
				}
			}
		}
	}
	if(0){
		MeshFiles debug_mesh;
		debug_mesh.positions.resize(mesh.positions.size());
		for (Index i = 0; i < mesh.positions.size(); ++i) {
			debug_mesh.positions[i] = mesh.positions[i];
		}
		debug_mesh.wedges.resize(mesh.wedges.size());
		for (Index i = 0; i < mesh.wedges.size(); ++i) {
			debug_mesh.wedges[i] = mesh.wedges[i];
		}
		debug_mesh.triangles.resize(mesh.triangles.size());
		for (Index i = 0; i < mesh.triangles.size(); ++i) {
			debug_mesh.triangles[i] = mesh.triangles[i];
		}
		export_obj(debug_mesh, "debug_after.obj");
	}
}

void simplify_mesh_edge(
	MergedMesh& mesh,
	Index target_triangle_count) {
	if (mesh.triangles.size() <= target_triangle_count || target_triangle_count == 0) {
		return;
	}
	std::vector<uint8_t> boundary_flag(mesh.positions.size(), 0);
	for (Index v : mesh.boundary_vertices) {
		boundary_flag[v] = 1;
	}
	struct Edge {
		Index v0, v1;
		float length;
		Edge() {}
		Edge(Index p0, Index p1, float l): v0(p0), v1(p1), length(l) {}
	};

	size_t n_triangles = mesh.triangles.size();
	std::vector<Edge> edges;
	while(n_triangles > target_triangle_count) {
		edges.clear();
		std::vector<Index> remap(mesh.positions.size());
		for(Index i = 0; i < remap.size(); i++)
			remap[i] = i;

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

			if(p1 > p0 && !boundary_flag[p0] && !boundary_flag[p1])
				edges.push_back(Edge(p0, p1, std::sqrt(dx01*dx01 + dy01*dy01 + dz01*dz01)));
			if(p2 > p1 && !boundary_flag[p1] && !boundary_flag[p2])
				edges.push_back(Edge(p1, p2, std::sqrt(dx12*dx12 + dy12*dy12 + dz12*dz12)));
			if(p2 > p0 && !boundary_flag[p0] && !boundary_flag[p2])
				edges.push_back(Edge(p0, p2, std::sqrt(dx20*dx20 + dy20*dy20 + dz20*dz20)));
		}
		std::sort(edges.begin(), edges.end(), [](const Edge &a, const Edge &b) { return a.length < b.length; });
		// Decide how many shortest edges to try collapsing this iteration.
		size_t remaining = (n_triangles > static_cast<size_t>(target_triangle_count)) ? (n_triangles - static_cast<size_t>(target_triangle_count)) : 0;
		if (edges.empty() || remaining == 0) break;
		size_t want = std::max((size_t)1, edges.size() / 10);
		size_t num_to_collapse = std::min(std::min(want, remaining), edges.size());
		edges.resize(num_to_collapse);
		std::vector<bool> lock(mesh.positions.size(), false);
		size_t collapsed_count = 0;
		for(const Edge &e: edges) {
			if(lock[e.v0] || lock[e.v1])
				continue;
			lock[e.v0] = lock[e.v1] = true;
			Vector3f &p0 = mesh.positions[e.v0];
			const Vector3f &p1 = mesh.positions[e.v1];

			Vector3f p;
			p.x = (p0.x + p1.x)/2.0f;
			p.y = (p0.y + p1.y)/2.0f;
			p.z = (p0.z + p1.z)/2.0f;
			p0 = p;
			remap[e.v1] = e.v0;
			++collapsed_count;
		}

		if (collapsed_count == 0) break;

		// Find root representative for remapped positions (path compression)
		auto find_root = [&](Index v) {
			Index r = v;
			while (remap[r] != r) r = remap[r];
			// compress
			Index x = v;
			while (remap[x] != r) {
				Index next = remap[x];
				remap[x] = r;
				x = next;
			}
			return r;
		};

		// Apply remapping to wedges so triangles refer to representative vertices
		for (Wedge &w : mesh.wedges) {
			w.p = find_root(w.p);
		}

		// Remove degenerate triangles (collapsed to fewer than 3 distinct positions)
		std::vector<Triangle> new_tris;
		new_tris.reserve(mesh.triangles.size());
		for (const Triangle &tri : mesh.triangles) {
			Index p0 = mesh.wedges[tri.w[0]].p;
			Index p1 = mesh.wedges[tri.w[1]].p;
			Index p2 = mesh.wedges[tri.w[2]].p;
			if (p0 == p1 || p1 == p2 || p0 == p2) continue;
			new_tris.push_back(tri);
		}
		mesh.triangles.swap(new_tris);
		n_triangles = mesh.triangles.size();
	}
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


	std::unordered_map<Index, const Wedge*> merged_wedge_for_pos;
	merged_wedge_for_pos.reserve(merged.wedges.size());
	for (const Wedge& w : merged.wedges) {
		if (merged_wedge_for_pos.find(w.p) == merged_wedge_for_pos.end()) {
			merged_wedge_for_pos[w.p] = &w;
		}
	}

	for (const auto& [original_pos, merged_pos] : merged.position_map) {
		next_mesh.positions[original_pos] = merged.positions[merged_pos];
	}
	for(Index b: merged.boundary_vertices) {
		Index i = merged.position_remap[b];
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

std::vector<Index> split_mesh(
	const MergedMesh& merged,
	MicroNode& micronode,
	MeshFiles& next_mesh,
	std::size_t max_triangles) {

	std::vector<Index> created_clusters;

	// Build merged_pos -> original_pos mapping
	const std::vector<Index> &merged_to_original = merged.position_remap;

	std::size_t num_triangles = merged.triangles.size();

	// Determine number of clusters to create
	std::size_t num_clusters = std::max((size_t)2, (num_triangles  +1) / max_triangles);
	if (num_clusters < 1) num_clusters = 1;
	if (num_clusters > num_triangles) num_clusters = num_triangles;

	std::vector<idx_t> part(num_triangles, 0);
	//TODO: we want to minimize the edges, we might want to take into account the edges length.
	if (num_clusters > 1) {
		// Build adjacency for METIS
		std::vector<FaceAdjacency> adjacency = build_adjacency_for_merged(merged);

		std::vector<idx_t> xadj;
		std::vector<idx_t> adjncy;
		std::vector<idx_t> adjwgt;

		xadj.reserve(num_triangles + 1);
		xadj.push_back(0);

		// Precompute triangle centroids for distance calculations
		std::vector<Vector3f> tri_centroids(num_triangles);
		for (std::size_t ti = 0; ti < num_triangles; ++ti) {
			const Triangle& tri = merged.triangles[ti];
			Vector3f c{0.0f, 0.0f, 0.0f};
			for (int k = 0; k < 3; ++k) {
				Index pidx = merged.wedges[tri.w[k]].p;
				const Vector3f& pp = merged.positions[pidx];
				c.x += pp.x; c.y += pp.y; c.z += pp.z;
			}
			c.x /= 3.0f; c.y /= 3.0f; c.z /= 3.0f;
			tri_centroids[ti] = c;
		}

		for (std::size_t i = 0; i < num_triangles; ++i) {
			const FaceAdjacency& adj = adjacency[i];
			for (int corner = 0; corner < 3; ++corner) {
				Index neighbor = adj.opp[corner];
				if (neighbor != std::numeric_limits<Index>::max()) {
					adjncy.push_back(static_cast<idx_t>(neighbor));

					// Compute edge length (shared edge between this triangle and neighbor)
					Index p0 = merged.wedges[merged.triangles[i].w[corner]].p;
					Index p1 = merged.wedges[merged.triangles[i].w[(corner + 1) % 3]].p;
					const Vector3f &pa = merged.positions[p0];
					const Vector3f &pb = merged.positions[p1];
					double dx = static_cast<double>(pa.x) - static_cast<double>(pb.x);
					double dy = static_cast<double>(pa.y) - static_cast<double>(pb.y);
					double dz = static_cast<double>(pa.z) - static_cast<double>(pb.z);
					double edge_len = std::sqrt(dx*dx + dy*dy + dz*dz);

					// Distance between triangle centroids
					const Vector3f &ca = tri_centroids[i];
					const Vector3f &cb = tri_centroids[neighbor];
					double dcx = static_cast<double>(ca.x) - static_cast<double>(cb.x);
					double dcy = static_cast<double>(ca.y) - static_cast<double>(cb.y);
					double dcz = static_cast<double>(ca.z) - static_cast<double>(cb.z);
					double cent_dist = std::sqrt(dcx*dcx + dcy*dcy + dcz*dcz);

					double raw_w = 0.0;
					if (cent_dist > 1e-12) raw_w = (edge_len / cent_dist) * 100.0;
					else raw_w = edge_len * 100.0; // fallback when centroids coincide

					idx_t iw = static_cast<idx_t>(std::lround(raw_w));
					if (iw < 1) iw = 1;
					adjwgt.push_back(iw);
				}
			}
			xadj.push_back(static_cast<idx_t>(adjncy.size()));
		}

		if (adjncy.empty()) {
			throw std::runtime_error("TOtally disconnected node");
		}
		if(!adjncy.empty()) {
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
				throw std::runtime_error("Failed to split cluster");
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

	//update positions
	for(size_t i = 0; i < merged.position_remap.size(); i++) {
		size_t original_pos = merged.position_remap[i];
		next_mesh.positions[original_pos] = merged.positions[i];
	}

	//TODO: this might be possibly problematic as wedges are not shared across miceronodes
	//update wedges
	Index wedge_offset = next_mesh.wedges.size();
	next_mesh.wedges.resize(wedge_offset + merged.wedges.size());
	for(int i = 0; i < merged.wedges.size(); i++) {
		Wedge w = merged.wedges[i];
		w.p = merged.position_remap[w.p];
		next_mesh.wedges[wedge_offset + i] = w;
	}
	MicroNode parent;

	// Create clusters in next_mesh
	for (std::size_t c = 0; c < actual_clusters; ++c) {
		std::vector<Index>& tri_indices = cluster_triangles[c];

		Index cluster_id = static_cast<Index>(next_mesh.clusters.size());
		Index triangle_offset = static_cast<Index>(next_mesh.triangles.size());
		Index triangle_count = static_cast<Index>(tri_indices.size());

		next_mesh.triangles.resize(triangle_offset + triangle_count);

		size_t count = 0;
		for (Index tri_idx : tri_indices) {
			Triangle tri = merged.triangles[tri_idx];
			for (int k = 0; k < 3; ++k) {
				tri.w[k] += wedge_offset;
			}
			next_mesh.triangles[triangle_offset + count++] = tri;
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
		parent.cluster_ids.push_back(cluster_id);
	}
	//we store the informatiin that these 2 cluster come from the same micronode in a new micronode.
	next_mesh.micronodes.push_back(parent);

	// Store created cluster indices in micronode.children_nodes (temporary)
	// This will be updated later to point to actual micronode IDs
	micronode.children_nodes.clear();
	for (Index cluster_id : created_clusters) {
		micronode.children_nodes.push_back(cluster_id);
	}

	return created_clusters;
}


} // namespace nx
