#ifndef BUILDMATERIAL_H
#define BUILDMATERIAL_H

#include <QString>
#include <vector>
#include <map>

#include "../common/material.h"

namespace nx {

class BuildMaterial: public Material {
public:
	int32_t atlas_offset;
	bool flipY = true;
	std::vector<QString> textures;
	std::map<int32_t, int8_t> countMaps();
};

class BuildMaterials: public std::vector<BuildMaterial> {
public:
	//map identical materials, it can be immediately mapped. //they always point back to a lower number..
	std::vector<int> material_map;
	//map materials with just a different texture, i need to keep them separated until we have created the nodes
	std::vector<int> texture_map;

	void unifyMaterials();
	void unifyTextures();

	//compact duplicated materials into an array and returns a mapping.
	std::vector<int> compact(std::vector<Material> &materials);
};

}

#endif // BUILDMATERIAL_H
