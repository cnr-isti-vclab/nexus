#include "material.h"

#include <math.h>

using namespace std;

//same exact parameters.
bool sameParameter(float a, float b) {
	return fabs(a - b) < 1/255.f;
}

bool sameColor(float *a, float *b) {
	for(int i = 0; i < 4; i++)
		if(fabs(a[i] - b[i]) > 1/225.0f) return false;
	return true;
}

bool sameMap(int8_t a, int8_t b) {
	return (a == -1 && b == -1) || (a >= 0 && b >= 0);
}

//Two materials where the ony difference is the actual texture.
bool textureCompatible(BuildMaterial &a, BuildMaterial &b) {
	return sameColor(a.ambient, b.ambient) &&
		sameColor(a.color, b.color) &&
		sameParameter(a.metallic, b.metallic) &&
		sameParameter(a.roughness, b.roughness) &&
		sameParameter(a.norm_scale, b.norm_scale) &&
		sameColor(a.specular, b.specular) &&
		sameParameter(a.glossiness, b.glossiness) &&
		sameParameter(a.occlusion, b.occlusion) &&
		a.nmaps == b.nmaps &&
		sameMap(a.color_map, b.color_map) &&
		sameMap(a.metallic_map, b.metallic_map) &&
		sameMap(a.normal_map, b.normal_map) &&
		sameMap(a.bump_map, b.bump_map) &&
		sameMap(a.specular_map, b.specular_map) &&
		sameMap(a.glossiness_map, b.glossiness_map) &&
		sameMap(a.occlusion_map, b.occlusion_map);
}

map<int32_t, int8_t> BuildMaterial::countMaps() {
	nmaps = 0;
	map<int32_t, int8_t> remap;

	int count = 0;
	if(color_map >= 0      && !remap.count(color_map))      remap[color_map] = count++;
	if(metallic_map >= 0   && !remap.count(metallic_map))   remap[metallic_map] = count++;
	if(normal_map >= 0     && !remap.count(normal_map))     remap[normal_map] = count++;
	if(bump_map >= 0       && !remap.count(bump_map))       remap[bump_map] = count++;
	if(specular_map >= 0   && !remap.count(specular_map))   remap[specular_map] = count++;
	if(glossiness_map >= 0 && !remap.count(glossiness_map)) remap[glossiness_map] = count++;
	if(occlusion_map >= 0  && !remap.count(occlusion_map))  remap[occlusion_map] = count++;

	nmaps = remap.size();
	return remap;
}



void BuildMaterials::unifyMaterials() {
	material_map.resize(size());
	for(size_t i = 0; i < size(); i++)
		material_map[i] = i;
}


void BuildMaterials::unifyTextures() {
	texture_map.resize(size());
	for(size_t i = 0; i < size(); i++) {
		BuildMaterial &source = at(i);
		texture_map[i] = i;
		for(size_t k = 0; k < i; k++) {
			BuildMaterial &target = at(k);
			if(textureCompatible(source, target))
				texture_map[i] = k;

		}
	}
}


std::vector<int> BuildMaterials::compact(std::vector<Material> &materials) {
	std::vector<int> final_map(size());
	for(size_t i = 0; i < size(); i++) {
		size_t m = texture_map[i];
		if(m == i) {
			final_map[i] = materials.size();
			materials.push_back(at(i));
		} else {
			final_map[i] = final_map[m];
		}
	}
	return final_map;
}
