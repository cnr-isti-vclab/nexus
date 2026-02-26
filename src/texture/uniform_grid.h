#pragma once

#include <vector>
#include <utility>

#include "../core/mappedmesh.h"

namespace nx {

class UniformTriangleGrid {
public:
	UniformTriangleGrid(const MappedMesh& mesh,
		const std::vector<std::pair<Index, Index>>& source_clusters);

	bool empty() const;

	bool project(const Vector3f& origin,
		const Vector3f& dir,
		Index& out_parent,
		Vector2f& out_uv,
		float& out_distance) const;

private:
	struct SourceTriangleRecord {
		Index parent_id = NONE;
		Vector3f a{0, 0, 0};
		Vector3f b{0, 0, 0};
		Vector3f c{0, 0, 0};
		Vector2f uv0{0, 0};
		Vector2f uv1{0, 0};
		Vector2f uv2{0, 0};
	};

	std::vector<SourceTriangleRecord> triangles;
	std::vector<std::vector<Index>> cells;
	Vector3f bounds_min{0, 0, 0};
	Vector3f bounds_max{1, 1, 1};
	Vector3f cell_size{1, 1, 1};
	Vector3f inv_cell_size{1, 1, 1};
	int dim_x = 1;
	int dim_y = 1;
	int dim_z = 1;

	static int clampCell(int value, int minv, int maxv);
	int cellIndex(int x, int y, int z) const;
	static float deltaT(float dir_component, float cell_extent);
	static float nextCrossingT(float origin_component,
		float dir_component,
		float min_component,
		float cell_extent,
		int cell,
		int step,
		float start_t);
	static bool intersectRayAabb(const Vector3f& origin,
		const Vector3f& dir,
		const Vector3f& bmin,
		const Vector3f& bmax,
		float& out_tmin,
		float& out_tmax);

	void build(const MappedMesh& mesh,
		const std::vector<std::pair<Index, Index>>& source_clusters);
};

} // namespace nx
