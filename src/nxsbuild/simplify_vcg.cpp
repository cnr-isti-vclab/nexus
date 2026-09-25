#include "merge_simplify_split.h"

#ifdef USE_VCG_SIMPLIFIER

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>
#include <vector>

#include "../core/mappedmesh.h"
#include "../core/vcgmesh.h"
#include "../loaders/objexporter.h"

namespace nx {

namespace {

float meshError(NodeMesh& mesh) {
	double length = 0;
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
		length += (dx01*dx01 + dy01*dy01 + dz01*dz01);
		length += (dx12*dx12 + dy12*dy12 + dz12*dz12);
		length += (dx20*dx20 + dy20*dy20 + dz20*dz20);
	}
	length = std::sqrt(length)/mesh.triangles.size()*3;
	return length;
}

}

float simplify_mesh(
	NodeMesh& merged,
	Index target_triangle_count) {

	if (merged.triangles.size() <= target_triangle_count || target_triangle_count == 0) {
		return 0;
	}

	if(0){
		MappedMesh debug_mesh;
		debug_mesh.positions.resize(merged.positions.size());
		for (Index i = 0; i < merged.positions.size(); ++i) {
			debug_mesh.positions[i] = merged.positions[i];
		}
		debug_mesh.wedges.resize(merged.wedges.size());
		for (Index i = 0; i < merged.wedges.size(); ++i) {
			debug_mesh.wedges[i] = merged.wedges[i];
		}
		debug_mesh.triangles.resize(merged.triangles.size());
		for (Index i = 0; i < merged.triangles.size(); ++i) {
			debug_mesh.triangles[i] = merged.triangles[i];
		}
		export_obj(debug_mesh, {}, "debug_before.obj");
	}

	VcgMesh legacy_mesh;
	vcg::tri::Allocator<VcgMesh>::AddVertices(legacy_mesh, merged.positions.size() * 3);
	vcg::tri::Allocator<VcgMesh>::AddFaces(legacy_mesh, merged.triangles.size());

	for (std::size_t i = 0; i < merged.positions.size(); ++i) {
		AVertex &v = legacy_mesh.vert[i];
		const Vector3f& p = merged.positions[i];
		v.P() = vcg::Point3f(p.x, p.y, p.z);
		v.C() = vcg::Color4b(255, 255, 255, 255);
	}

	for (Index b : merged.boundary_vertices) {
		legacy_mesh.vert[b].ClearW();
	}

	for (std::size_t i = 0; i < merged.triangles.size(); ++i) {
		const Triangle& tri = merged.triangles[i];
		AFace& face = legacy_mesh.face[i];
		for (int k = 0; k < 3; ++k) {
			const Wedge& w = merged.wedges[tri.w[k]];
			face.V(k) = &legacy_mesh.vert[w.p];
		}
	}
	vcg::tri::UpdateNormal<VcgMesh>::PerVertex(legacy_mesh);
	legacy_mesh.quadricInit();
	legacy_mesh.simplify(target_triangle_count, VcgMesh::QUADRICS);

	std::vector<Index> vertex_remap(merged.positions.size(), NONE);

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
		merged.position_remap[count] = merged.position_remap[i];
		merged.positions[count++] = { p[0], p[1], p[2]};
	}
	merged.positions.resize(count);

	for(Index &i: merged.boundary_vertices)
		i = vertex_remap[i];

	if (!merged.boundary_vertices.empty()) {
		merged.boundary_vertices.erase(
			std::remove_if(merged.boundary_vertices.begin(), merged.boundary_vertices.end(),
						   [](Index v){ return v == std::numeric_limits<Index>::max(); }),
			merged.boundary_vertices.end());

		if (!merged.boundary_vertices.empty()) {
			std::sort(merged.boundary_vertices.begin(), merged.boundary_vertices.end());
			merged.boundary_vertices.erase(std::unique(merged.boundary_vertices.begin(), merged.boundary_vertices.end()),
										 merged.boundary_vertices.end());
		}
	}

	merged.wedges.resize(merged.positions.size());
	for(size_t i = 0; i < merged.wedges.size(); i++) {
		auto &w = merged.wedges[i];
		w.p =  i;
		w.n = NONE;
		w.t = NONE;
	}

	merged.triangles.clear();

	for (std::size_t i = 0; i < legacy_mesh.face.size(); ++i) {
		AFace& f = legacy_mesh.face[i];
		if (f.IsD()) {
			continue;
		}

		Triangle tri;
		for (int k = 0; k < 3; ++k) {
			const AVertex* v = f.cV(k);
			std::size_t vidx = static_cast<std::size_t>(v - vbase);
			Index new_pos = vertex_remap[vidx];
			assert(new_pos != NONE);
			tri.w[k] = new_pos;
		}
		merged.triangles.push_back(tri);
	}

	if(0){
		MappedMesh debug_mesh;
		debug_mesh.positions.resize(merged.positions.size());
		for (Index i = 0; i < merged.positions.size(); ++i) {
			debug_mesh.positions[i] = merged.positions[i];
		}
		debug_mesh.wedges.resize(merged.wedges.size());
		for (Index i = 0; i < merged.wedges.size(); ++i) {
			debug_mesh.wedges[i] = merged.wedges[i];
		}
		debug_mesh.triangles.resize(merged.triangles.size());
		for (Index i = 0; i < merged.triangles.size(); ++i) {
			debug_mesh.triangles[i] = merged.triangles[i];
		}
		export_obj(debug_mesh, {}, "debug_after.obj");
	}
	return meshError(merged);
}

}

#endif
