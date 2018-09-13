#ifndef GLTF_H
#define GLTF_H

#include <QString>
#include <vector>

#include <QJsonObject>

/*
std::vector<tinygltf::Image> images;               //for each image wwe need bufferView and mimeTYpe ("image/jpeg")
std::vector<tinygltf::Texture> textures;           //we need sampler and source is the index of the image
std::vector<tinygltf::BufferView> bufferviews;      //buffer number, byteLength, byteOffset by default 0, target ["ARRAY_BUFFER", "ELEMENT_ARRAY_BUFFER"]
std::vector<tinygltf::Accessor> accessors; //int bufferView, byteoffset //used for primitives, normalized, componentType TINYGLTF_COMPONENT_TYPE_BYTE, count, type TINYGLTF_TYPE_VEC2 etc.
std::vector<tinygltf::Primitive> primitives; //attributes NORMAL: index of the accessor., material (needed or use parent?), indices(accessor again), mode
std::vector<tinygltf::Mesh> meshes; //primitives array //one per patch!
*/

class gltfElement: public QJsonObject {

};


#endif // GLTF_H

