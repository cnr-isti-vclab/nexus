#ifndef MATERIAL_H
#define MATERIAL_H


#include "config.h"

/* Material loading strategy:
 * Create list of materials (1 for texture in ply, might be more in Obj or gltf)
 * Each triangle is tagged with the material + material_offset (in case we load more than one mesh)
 * Unify materials which are exatly the same create a material map.
 * Create another map to unify materials with same parameters (and just different texture) will be used in the nodetex
 */


/*  Plan for version 3
 *
 * Add support for multiple materials:
 * 1) we need to update the header to store the list of the materials.
 * 2) loader must be able to create the materials:
 *      (if the material are the same except for the texture, we merge them!)
 *      (if the material are different but have the same type and number of textures, we separate the materials but merge the textures)
 *      material creation as soon as loader load(filename).
 *      if we merge we must record each triangle both material and texture.
 *      we could do that at the end with a map-material -> (merged material, texture).
 * 3) simplification must preserve material border.
 * 4) patch must specify material AND texture.
 * 5) threejs loader will create the materials from the  header, and load the textures when needed.
 */

//WARNING: occlusion roughness metallic can go in the same texture (metallic takes blue, roughness green, occlusion red)

/* Flexibility needed:

1: since now we have material/texture and texture per node, we can have patches using the parent node texture,
so we can save a texture every 2 levels.

2 if we have too much texture respect the geometry, we can
	a) skipping simplification. (geometry will have little weight, implementing is trivial but this require stupid reparametrization.)
	   it would require to start with nodes with less geometry,  (so initial node primitives,vs standard node primitives).
	   if we had an area associated to a triangle, we could use that in kdtree instead of jus the number of triangles
	b) node becomes virtual: they point to geometry, we need nodes, patches, geometry and textures.
	   how this could work with different resolution in different part of the model?!
*/

class Material {
public:                               // gltf                  //obj

	float ambient[4] = { 0, 0, 0, 0};
	float color[4] = { 0, 0, 0, 0};// baseColorFactor       //kd

	float metallic = 0;            //metallicFactor
	float roughness = 0;           //roughnessFactor

	float norm_scale = 0.0f;        //normalTexture scale
	float specular[4] = { 0, 0, 0, 0};                                     //Ks
	float glossiness = 0.0f;                                  //Ns
	float occlusion = 0.0f;         //occlusionTexture->strength

	uint8_t nmaps = 0;

	//TODO this should best be an array with enum index or just use gltf material
	//map number in texture group.
	int8_t color_map = -1;          // baseColorTexture      //map_kd
	int8_t metallic_map = -1;       //metallicRoughnessTexture
	int8_t normal_map = -1;           //normalTextuire         //norm
	int8_t bump_map = -1;           //                       //bump bumb_map
	int8_t specular_map = -1;                                //map_ks
	int8_t glossiness_map = -1;                               //
	int8_t occlusion_map = -1;             //occlustionTexture

};




#endif // MATERIAL_H
