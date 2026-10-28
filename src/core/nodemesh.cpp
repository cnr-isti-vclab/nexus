#include "nodemesh.h"

namespace nx {

float NodeMesh::meshError(const NodeMesh& mesh) {
	if(mesh.triangles.empty())
		return 0.0f;

	double squared_edge_length_sum = 0.0;
	for(const Triangle& triangle : mesh.triangles) {
		const Index p0 = mesh.wedges[triangle.w[0]].p;
		const Index p1 = mesh.wedges[triangle.w[1]].p;
		const Index p2 = mesh.wedges[triangle.w[2]].p;
		const Vector3f& a = mesh.positions[p0];
		const Vector3f& b = mesh.positions[p1];
		const Vector3f& c = mesh.positions[p2];
		const double dx01 = double(a.x) - b.x;
		const double dy01 = double(a.y) - b.y;
		const double dz01 = double(a.z) - b.z;
		const double dx12 = double(b.x) - c.x;
		const double dy12 = double(b.y) - c.y;
		const double dz12 = double(b.z) - c.z;
		const double dx20 = double(c.x) - a.x;
		const double dy20 = double(c.y) - a.y;
		const double dz20 = double(c.z) - a.z;
		squared_edge_length_sum += dx01*dx01 + dy01*dy01 + dz01*dz01;
		squared_edge_length_sum += dx12*dx12 + dy12*dy12 + dz12*dz12;
		squared_edge_length_sum += dx20*dx20 + dy20*dy20 + dz20*dz20;
	}
	return static_cast<float>(std::sqrt(squared_edge_length_sum) * 3.0 / mesh.triangles.size());
}

}
