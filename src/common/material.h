#ifndef MATERIAL_H
#define MATERIAL_H

//TODO move this include in a config.h include.
#ifdef _MSC_VER

typedef __int16 int16_t;
typedef unsigned __int16 uint16_t;
typedef __int32 int32_t;
typedef unsigned __int32 uint32_t;
typedef __int64 int64_t;
typedef unsigned __int64 uint64_t;

#else
#include <stdint.h>
#endif

#include <QString>
#include <vector>

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
public:                              // gltf                  //obj

	uint8_t color[4] = { 0, 0, 0, 0};                // baseColorFactor       //kd
	uint8_t color_map = -1;          // baseColorTexture      //map_kd

	uint8_t metallic = 0;            //metallicFactor
	uint8_t roughness = 0;           //roughnessFactor
	uint8_t metallic_map = 255;       //metallicRoughnessTexture

	uint8_t norm_scale = 255;        //normalTexture scale
	uint8_t norm_map = -1;           //normalTextuire         //norm
	uint8_t bump_map = -1;           //                       //bump bumb_map

	uint8_t specular[4] = { 0, 0, 0, 0};                                     //Ks
	uint8_t specular_map = 255;                                //map_ks

	uint8_t glossines = 255;                                  //Ns
	uint8_t glossines_map = 255;                               //

	uint8_t occlusion = 255;         //occlusionTexture->strength
	uint8_t occlusion_map = 255;             //occlustionTexture
};

class BuildMaterial: public Material {
public:
	std::vector<QString> textures;
};



#endif // MATERIAL_H
