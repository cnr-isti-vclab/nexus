#include "tileset.h"
#include <QTextStream>
#include <fstream>

std::array<float, 12> convertBoundingBox(const vcg::Point3f &X, const vcg::Point3f &Y, const vcg::Point3f &Z,
										 const vcg::Point3f &min, const vcg::Point3f &max) {
	vcg::Point3f centre = (min + max) * 0.5;
	float halfLengthX = (max.X() - min.X())/2;
	vcg::Point3f halfAxeX = X * halfLengthX;
	float halfLengthY = (max.Y() - min.Y())/2;
	vcg::Point3f halfAxeY = Y * halfLengthY;
	float halfLengthZ = (max.Z() - min.Z())/2;
	vcg::Point3f halfAxeZ = Z * halfLengthZ;

	std::array<float, 12> bbox = { centre.X(),    centre.Y(),   centre.Z(),
		halfAxeX.X(),  halfAxeX.Y(), halfAxeX.Z(),
		halfAxeY.X(),  halfAxeY.Y(), halfAxeY.Z(),
		halfAxeZ.X(),  halfAxeZ.Y(), halfAxeZ.Z()
	};

	return bbox;
}


void write(const nlohmann::json &json, const QString &path) {

	std::ofstream file(path.toStdString(), std::ofstream::out);
	file << json;
	file.close();
}

void TilesetJSON::generate() {
	for(uint i = 0; i < m_nexus.nodes.size() - 1; i++)
		makeTileObject(i);

	addChildren();
	Tileset tileset;
	tileset.root = m_tiles[0];
	write(tileset.toJson(), "output/tileset.json");
}

void TilesetJSON::makeTileObject(int tileIndex) {

	Tile tile;

			// 1. Extract bounding volume and convert it in 3D tiles boundingVolume specification.
	auto axes = m_nexus.boxes[tileIndex].axes;
	auto box = m_nexus.boxes[tileIndex].box;
	auto bbox = convertBoundingBox(axes[0], axes[1], axes[2], box.min, box.max);
	tile.boundingVolume.box = bbox;

			// 2. GeometricError
	nx::Node &node = m_nexus.nodes[tileIndex];
	tile.geometricError = node.error;
	// are they the same values ??

			// 3. Uri
	tile.content.uri = std::to_string(tileIndex) + ".b3dm";

			// 4. Extract children;

	for(int i = node.first_patch; i < node.last_patch(); i++) {
		const nx::Patch &patch = m_nexus.patches[i];
		if(!(patch.node == m_nexus.nodes.size()-1)){
			tile.childrenIndex.push_back(patch.node);
		}
	}

	m_tiles.push_back(tile);
}

void TilesetJSON::addChildren() {

	for( int i = m_tiles.size() - 1; i >= 0; i-- ) {
		for(int child_index : m_tiles[i].childrenIndex) {
			m_tiles[i].children.push_back(m_tiles[child_index]);
		}
	}
}


void TilesetJSON::writeMinimalTilesetJSON(int node_index) {

	nx::Node &node = m_nexus.nodes[node_index];
	Tile simpleTile;

	auto axes = m_nexus.boxes[node_index].axes;
	auto box = m_nexus.boxes[node_index].box;
	simpleTile.boundingVolume.box = convertBoundingBox(axes[0], axes[1], axes[2], box.min, box.max);
	simpleTile.geometricError = node.error;
	simpleTile.content.uri = std::to_string(node_index) + ".b3dm";

	Tileset tileset;
	tileset.root = simpleTile;

	write(tileset.toJson(), "output/tileset2.json");
}
