#ifndef BUILDMATERIAL_H
#define BUILDMATERIAL_H

#include <QString>
#include <vector>
#include <map>

#include "../core/material.h"

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
    // map identical materials; entries point to a lower index when unified
    std::vector<int> material_map;
    // map materials with just a different texture
    std::vector<int> texture_map;

    void unifyMaterials();
    void unifyTextures();

    // compact duplicated materials into an array and return the mapping
    std::vector<int> compact(std::vector<Material>& materials);
};

} // namespace nx

#endif // BUILDMATERIAL_H
