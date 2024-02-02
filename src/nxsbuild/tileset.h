#ifndef TILESET_H
#define TILESET_H

#include "deps/json.hpp"
#include "nexusbuilder.h"
#include <string>
#include <vector>

struct BoundingVolume
{
	std::array<float, 12> box = {0};

	nlohmann::json toJson() {
		nlohmann::json j;
		j ={ {"box", box }};
		return j;
	}
};

struct Content
{
	BoundingVolume boundingVolume;
	bool boundVolDefined = false;
	std::string uri = "";
	nlohmann::json toJson() {
		nlohmann::json j;
		if(boundVolDefined)
			j = { {"uri", uri}, {"boundingVolume", boundingVolume.toJson()}};
		else
			j = { {"uri", uri} };
		return j;
	}
};

struct Tile
{
	float transform[16] = { 1.0, 0.0, 0.0, 0.0,
						   0.0, 1.0, 0.0, 0.0,
						   0.0, 0.0, 1.0, 0.0,
						   0.0, 0.0, 0.0, 1.0};
	bool transformDefined = false;
	BoundingVolume boundingVolume;
	float geometricError = 0;
	std::string refine = "REPLACE";
	std::vector<Tile> children;
	Content content;
	std::vector<int> childrenIndex;

	nlohmann::json toJson() {
		std::vector<nlohmann::json> temp;
		for (Tile child : children) {
			temp.push_back(child.toJson());
		}

		nlohmann::json j;

		if (transformDefined) j["transform"] = transform;
		j["boundingVolume"] = boundingVolume.toJson();
		j["geometricError"] = geometricError;
		j["content"] = content.toJson();
		if (refine != "") j["refine"] = refine;
		if (temp.size()) j["children"] = temp;
		return j;
	}
};

struct Tileset
{
	std::string version = "1.0";
	int geometricError = 500;
	Tile root;

	nlohmann::json toJson() {
		nlohmann::json j;
		nlohmann::json temp;
		temp["version"] = version;
		j["asset"] = temp;
		j["geometricError"] = geometricError;
		j["root"] = root.toJson();
		return j;
	}
};

class TilesetJSON
{
	public:
	TilesetJSON() = delete;
	TilesetJSON(NexusBuilder &nexus) : m_nexus(nexus) {};
	void generate();

			//debug
	void writeMinimalTilesetJSON(int node_index);

	private:
	NexusBuilder& m_nexus;
	std::vector<Tile> m_tiles;

	void makeTileObject(int tile_index);
	void addChildren();
};

#endif // TILESET_H
