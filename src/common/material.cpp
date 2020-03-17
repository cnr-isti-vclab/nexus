#include "material.h"

using namespace std;


void BuildMaterials::unifyMaterials() {
	material_map.resize(size());
	for(int i = 0; i < size(); i++)
		material_map[i] = i;
}

void BuildMaterials::unifyTextures() {
	texture_map.resize(size());
	for(int i = 0; i < size(); i++)
		texture_map[i] = i;
}

std::vector<int> BuildMaterials::compact(std::vector<Material> &materials) {
	std::vector<int> final_map(size());
	for(int i = 0; i < size(); i++) {
		int m = texture_map[i];
		if(m == i) {
			final_map[i] = materials.size();
			materials.push_back(at(i));
		} else {
			final_map[i] = final_map[m];
		}
	}
	return final_map;
}
