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
