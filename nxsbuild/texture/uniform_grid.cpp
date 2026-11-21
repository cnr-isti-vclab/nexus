#include "uniform_grid.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>

namespace nx {

namespace {

inline Vector3f add(const Vector3f& a, const Vector3f& b) {
	return {a.x + b.x, a.y + b.y, a.z + b.z};
}

inline Vector3f sub(const Vector3f& a, const Vector3f& b) {
	return {a.x - b.x, a.y - b.y, a.z - b.z};
}

inline Vector3f mul(const Vector3f& v, float s) {
	return {v.x * s, v.y * s, v.z * s};
}

inline float dot(const Vector3f& a, const Vector3f& b) {
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline Vector3f cross(const Vector3f& a, const Vector3f& b) {
	return {
		a.y * b.z - a.z * b.y,
		a.z * b.x - a.x * b.z,
		a.x * b.y - a.y * b.x
	};
}

inline float length(const Vector3f& v) {
	return std::sqrt(dot(v, v));
}

bool intersect_ray_triangle(const Vector3f& origin,
	const Vector3f& dir,
	const Vector3f& a,
	const Vector3f& b,
	const Vector3f& c,
	float& t,
	float& bary_u,
	float& bary_v) {
	const Vector3f e1 = sub(b, a);
	const Vector3f e2 = sub(c, a);
	const Vector3f p = cross(dir, e2);
	const float det = dot(e1, p);

	const float inv_det = 1.0f / det;
	const Vector3f s = sub(origin, a);
	bary_u = dot(s, p) * inv_det;
	if(bary_u < 0.0f || bary_u > 1.0f)
		return false;

	const Vector3f q = cross(s, e1);
	bary_v = dot(dir, q) * inv_det;
	if(bary_v < 0.0f || (bary_u + bary_v) > 1.0f)
		return false;

	t = std::fabs(dot(e2, q) * inv_det);
	return true;
}

} // namespace

UniformTriangleGrid::UniformTriangleGrid(const MappedMesh& mesh,
	const std::vector<std::pair<Index, Index>>& source_clusters) {
	build(mesh, source_clusters);
}

bool UniformTriangleGrid::empty() const {
	return triangles.empty();
}

bool UniformTriangleGrid::project(const Vector3f& origin,
	const Vector3f& dir,
	Index& out_parent,
	Vector2f& out_uv,
	float& out_distance) const {
	if(triangles.empty())
		return false;

	float t_entry = 0.0f;
	float t_exit = 0.0f;
	if(!intersectRayAabb(origin, dir, bounds_min, bounds_max, t_entry, t_exit))
		return false;

	if(t_exit < 0.0f)
		return false;

	float t = std::max(0.0f, t_entry);
	Vector3f p = add(origin, mul(dir, t));
	int cx = clampCell(static_cast<int>((p.x - bounds_min.x) * inv_cell_size.x), 0, dim_x - 1);
	int cy = clampCell(static_cast<int>((p.y - bounds_min.y) * inv_cell_size.y), 0, dim_y - 1);
	int cz = clampCell(static_cast<int>((p.z - bounds_min.z) * inv_cell_size.z), 0, dim_z - 1);

	int step_x = (dir.x > 0.0f) ? 1 : ((dir.x < 0.0f) ? -1 : 0);
	int step_y = (dir.y > 0.0f) ? 1 : ((dir.y < 0.0f) ? -1 : 0);
	int step_z = (dir.z > 0.0f) ? 1 : ((dir.z < 0.0f) ? -1 : 0);

	float next_tx = nextCrossingT(origin.x, dir.x, bounds_min.x, cell_size.x, cx, step_x, t);
	float next_ty = nextCrossingT(origin.y, dir.y, bounds_min.y, cell_size.y, cy, step_y, t);
	float next_tz = nextCrossingT(origin.z, dir.z, bounds_min.z, cell_size.z, cz, step_z, t);

	float delta_tx = deltaT(dir.x, cell_size.x);
	float delta_ty = deltaT(dir.y, cell_size.y);
	float delta_tz = deltaT(dir.z, cell_size.z);

	float best_distance = std::numeric_limits<float>::max();
	bool found = false;
	std::vector<uint8_t> visited(triangles.size(), 0);

	while(cx >= 0 && cx < dim_x && cy >= 0 && cy < dim_y && cz >= 0 && cz < dim_z) {
		const int cell_id = cellIndex(cx, cy, cz);
		const std::vector<Index>& entries = cells[cell_id];
		for(Index tri_id: entries) {
			if(visited[tri_id])
				continue;
			visited[tri_id] = 1;

			const SourceTriangleRecord& tri = triangles[tri_id];
			float t_hit = 0.0f;
			float u = 0.0f;
			float v = 0.0f;
			if(!intersect_ray_triangle(origin, dir, tri.a, tri.b, tri.c, t_hit, u, v))
				continue;
			if(t_hit >= best_distance)
				continue;

			const float w = 1.0f - u - v;
			out_uv.u = tri.uv0.u * w + tri.uv1.u * u + tri.uv2.u * v;
			out_uv.v = tri.uv0.v * w + tri.uv1.v * u + tri.uv2.v * v;
			out_parent = tri.parent_id;
			best_distance = t_hit;
			found = true;
		}

		float next_boundary_t = std::min(next_tx, std::min(next_ty, next_tz));
		if(found && best_distance <= next_boundary_t)
			break;
		if(next_boundary_t > t_exit)
			break;

		if(next_tx <= next_ty && next_tx <= next_tz) {
			cx += step_x;
			next_tx += delta_tx;
		} else if(next_ty <= next_tx && next_ty <= next_tz) {
			cy += step_y;
			next_ty += delta_ty;
		} else {
			cz += step_z;
			next_tz += delta_tz;
		}
	}

	if(!found)
		return false;
	out_distance = best_distance;
	return true;
}

int UniformTriangleGrid::clampCell(int value, int minv, int maxv) {
	if(value < minv)
		return minv;
	if(value > maxv)
		return maxv;
	return value;
}

int UniformTriangleGrid::cellIndex(int x, int y, int z) const {
	return x + dim_x * (y + dim_y * z);
}

float UniformTriangleGrid::deltaT(float dir_component, float cell_extent) {
	if(std::fabs(dir_component) < 1e-20f)
		return std::numeric_limits<float>::infinity();
	return std::fabs(cell_extent / dir_component);
}

float UniformTriangleGrid::nextCrossingT(float origin_component,
	float dir_component,
	float min_component,
	float cell_extent,
	int cell,
	int step,
	float start_t) {
	if(step == 0)
		return std::numeric_limits<float>::infinity();
	const float boundary = min_component + ((step > 0) ? (static_cast<float>(cell + 1) * cell_extent)
		: (static_cast<float>(cell) * cell_extent));
	float t = (boundary - origin_component) / dir_component;
	if(t < start_t)
		t = start_t;
	return t;
}

bool UniformTriangleGrid::intersectRayAabb(const Vector3f& origin,
	const Vector3f& dir,
	const Vector3f& bmin,
	const Vector3f& bmax,
	float& out_tmin,
	float& out_tmax) {
	float tmin = -std::numeric_limits<float>::infinity();
	float tmax = std::numeric_limits<float>::infinity();

	auto update_axis = [&](float o, float d, float mn, float mx) -> bool {
		if(std::fabs(d) < 1e-20f)
			return o >= mn && o <= mx;
		float inv = 1.0f / d;
		float t0 = (mn - o) * inv;
		float t1 = (mx - o) * inv;
		if(t0 > t1)
			std::swap(t0, t1);
		tmin = std::max(tmin, t0);
		tmax = std::min(tmax, t1);
		return tmin <= tmax;
	};

	if(!update_axis(origin.x, dir.x, bmin.x, bmax.x))
		return false;
	if(!update_axis(origin.y, dir.y, bmin.y, bmax.y))
		return false;
	if(!update_axis(origin.z, dir.z, bmin.z, bmax.z))
		return false;

	out_tmin = tmin;
	out_tmax = tmax;
	return true;
}

void UniformTriangleGrid::build(const MappedMesh& mesh,
	const std::vector<std::pair<Index, Index>>& source_clusters) {
	triangles.clear();
	triangles.reserve(source_clusters.size() * 64);

	bool first = true;
	Vector3f local_min{0, 0, 0};
	Vector3f local_max{0, 0, 0};
	float total_edge = 0.0f;
	size_t edge_count = 0;

	for(const auto& ref: source_clusters) {
		const Index parent_id = ref.first;
		const Index cluster_id = ref.second;
		assert(cluster_id < mesh.clusters.size());
		const Cluster& cluster = mesh.clusters[cluster_id];

		for(Index tri_idx = cluster.triangle_offset; tri_idx < cluster.triangle_offset + cluster.triangle_count; ++tri_idx) {
			assert(tri_idx < mesh.triangles.size());
			const Triangle& tri = mesh.triangles[tri_idx];
			const Wedge& w0 = mesh.wedges[tri.w[0]];
			const Wedge& w1 = mesh.wedges[tri.w[1]];
			const Wedge& w2 = mesh.wedges[tri.w[2]];
			assert(w0.p < mesh.positions.size());
			assert(w1.p < mesh.positions.size());
			assert(w2.p < mesh.positions.size());
			assert(w0.t < mesh.texcoords.size());
			assert(w1.t < mesh.texcoords.size());
			assert(w2.t < mesh.texcoords.size());

			SourceTriangleRecord record;
			record.parent_id = parent_id;
			record.a = mesh.positions[w0.p];
			record.b = mesh.positions[w1.p];
			record.c = mesh.positions[w2.p];
			record.uv0 = mesh.texcoords[w0.t];
			record.uv1 = mesh.texcoords[w1.t];
			record.uv2 = mesh.texcoords[w2.t];
			triangles.push_back(record);

			const Vector3f tri_min{
				std::min(record.a.x, std::min(record.b.x, record.c.x)),
				std::min(record.a.y, std::min(record.b.y, record.c.y)),
				std::min(record.a.z, std::min(record.b.z, record.c.z))
			};
			const Vector3f tri_max{
				std::max(record.a.x, std::max(record.b.x, record.c.x)),
				std::max(record.a.y, std::max(record.b.y, record.c.y)),
				std::max(record.a.z, std::max(record.b.z, record.c.z))
			};
			if(first) {
				local_min = tri_min;
				local_max = tri_max;
				first = false;
			} else {
				local_min.x = std::min(local_min.x, tri_min.x);
				local_min.y = std::min(local_min.y, tri_min.y);
				local_min.z = std::min(local_min.z, tri_min.z);
				local_max.x = std::max(local_max.x, tri_max.x);
				local_max.y = std::max(local_max.y, tri_max.y);
				local_max.z = std::max(local_max.z, tri_max.z);
			}

			total_edge += length(sub(record.b, record.a));
			total_edge += length(sub(record.c, record.b));
			total_edge += length(sub(record.a, record.c));
			edge_count += 3;
		}
	}

	if(triangles.empty()) {
		dim_x = dim_y = dim_z = 1;
		cells.assign(1, {});
		bounds_min = {0, 0, 0};
		bounds_max = {1, 1, 1};
		cell_size = {1, 1, 1};
		inv_cell_size = {1, 1, 1};
		return;
	}

	bounds_min = local_min;
	bounds_max = local_max;
	const float eps = 1e-5f;
	if(bounds_max.x - bounds_min.x < eps)
		bounds_max.x = bounds_min.x + eps;
	if(bounds_max.y - bounds_min.y < eps)
		bounds_max.y = bounds_min.y + eps;
	if(bounds_max.z - bounds_min.z < eps)
		bounds_max.z = bounds_min.z + eps;

	const float avg_edge = (edge_count > 0) ? (total_edge / static_cast<float>(edge_count)) : 0.01f;
	const Vector3f extent = sub(bounds_max, bounds_min);
	dim_x = std::clamp(static_cast<int>(std::ceil(extent.x / std::max(avg_edge, eps))), 8, 64);
	dim_y = std::clamp(static_cast<int>(std::ceil(extent.y / std::max(avg_edge, eps))), 8, 64);
	dim_z = std::clamp(static_cast<int>(std::ceil(extent.z / std::max(avg_edge, eps))), 8, 64);

	cell_size = {
		extent.x / static_cast<float>(dim_x),
		extent.y / static_cast<float>(dim_y),
		extent.z / static_cast<float>(dim_z)
	};
	inv_cell_size = {
		1.0f / cell_size.x,
		1.0f / cell_size.y,
		1.0f / cell_size.z
	};

	cells.assign(static_cast<size_t>(dim_x) * static_cast<size_t>(dim_y) * static_cast<size_t>(dim_z), {});

	for(Index tri_id = 0; tri_id < triangles.size(); ++tri_id) {
		const SourceTriangleRecord& tri = triangles[tri_id];
		const Vector3f tri_min{
			std::min(tri.a.x, std::min(tri.b.x, tri.c.x)),
			std::min(tri.a.y, std::min(tri.b.y, tri.c.y)),
			std::min(tri.a.z, std::min(tri.b.z, tri.c.z))
		};
		const Vector3f tri_max{
			std::max(tri.a.x, std::max(tri.b.x, tri.c.x)),
			std::max(tri.a.y, std::max(tri.b.y, tri.c.y)),
			std::max(tri.a.z, std::max(tri.b.z, tri.c.z))
		};

		const int min_x = clampCell(static_cast<int>((tri_min.x - bounds_min.x) * inv_cell_size.x), 0, dim_x - 1);
		const int min_y = clampCell(static_cast<int>((tri_min.y - bounds_min.y) * inv_cell_size.y), 0, dim_y - 1);
		const int min_z = clampCell(static_cast<int>((tri_min.z - bounds_min.z) * inv_cell_size.z), 0, dim_z - 1);
		const int max_x = clampCell(static_cast<int>((tri_max.x - bounds_min.x) * inv_cell_size.x), 0, dim_x - 1);
		const int max_y = clampCell(static_cast<int>((tri_max.y - bounds_min.y) * inv_cell_size.y), 0, dim_y - 1);
		const int max_z = clampCell(static_cast<int>((tri_max.z - bounds_min.z) * inv_cell_size.z), 0, dim_z - 1);

		for(int z = min_z; z <= max_z; ++z)
			for(int y = min_y; y <= max_y; ++y)
				for(int x = min_x; x <= max_x; ++x)
					cells[cellIndex(x, y, z)].push_back(tri_id);
	}
}

} // namespace nx
