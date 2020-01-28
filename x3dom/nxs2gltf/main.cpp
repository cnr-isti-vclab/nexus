/*
Nexus

Copyright(C) 2012 - Federico Ponchio
ISTI - Italian National Research Council - Visual Computing Lab

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License (http://www.gnu.org/licenses/gpl.txt)
for more details.
*/

#include <stdio.h>
#include <iostream>

#include <QStringList>
#include <wrap/system/qgetopt.h>
#include "../../src/common/nexusdata.h"

#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include "tiny_gltf.h"
using namespace std;
using namespace nx;



int main(int argc, char *argv[]) {

	if(argc != 3) {
		cerr << "Usage: " << argv[0] << " <.nxs> <.gtb> [options incoming...]\n";
		return 0;
	}

	NexusData nexus;
	if(!nexus.open(argv[1])) {
		cerr << "Could not load nxs: " << argv[1] << endl;
		return 0;
	}



	tinygltf::Sampler sampler;
	sampler.minFilter = TINYGLTF_TEXTURE_FILTER_LINEAR;
	sampler.magFilter = TINYGLTF_TEXTURE_FILTER_LINEAR;
	sampler.wrapS = TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE;
	sampler.wrapT = TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE;


	//buffer is the entire nexus, so we just can use the existing
	int n_nodes = nexus.header.n_nodes;
	Signature &signature = nexus.header.signature;
	bool has_index = signature.face.hasIndex();
	bool has_textures = signature.vertex.hasTextures();


	//Buffer

/*	nodes.resize(n_nodes);
	meshes.resize(n_nodes-1);
	scenes.resize(1);
	model.defaultScene = 0;
	auto &scene = scenes[0];
	scene.nodes.resize(1, n_nodes-1);
	auto &topnode = nodes[n_nodes-1]; //last nodes */

	for(int n = 0; n < n_nodes-1; n++) {
		Node &node = nexus.nodes[n];
		nexus.loadRam(n);
		NodeData &data = nexus.nodedata[n]; //now it's loaded

		tinygltf::Model model;
		auto &buffers     = model.buffers;
		auto &bufferViews = model.bufferViews;      //buffer number, byteLength, byteOffset by default 0, target ["ARRAY_BUFFER", "ELEMENT_ARRAY_BUFFER"]
		auto &accessors   = model.accessors; //int bufferView, byteoffset //used for primitives, normalized, componentType TINYGLTF_COMPONENT_TYPE_BYTE, count, type TINYGLTF_TYPE_VEC2 etc.
		auto &meshes = model.meshes; //primitives array //one per patch!
		auto &nodes = model.nodes;
		auto &scenes = model.scenes;


		tinygltf::Mesh mesh;


		/* BUFFERVIEWS */

		int buffer_offset = 0;
		int tot_buffer_size = 0;

		int vert_index = bufferViews.size();
		tinygltf::BufferView vertbuffer;
		vertbuffer.buffer = buffers.size();
		vertbuffer.byteLength = node.nvert*signature.vertex.size();
		vertbuffer.byteOffset = 0; //node.getBeginOffset();
		vertbuffer.target = TINYGLTF_TARGET_ARRAY_BUFFER;
		bufferViews.push_back(vertbuffer);
		buffer_offset += vertbuffer.byteLength;

		int face_index = bufferViews.size();
		if(has_index) {
			tinygltf::BufferView facebuffer;
			facebuffer.buffer = buffers.size();
			facebuffer.byteLength = node.nface*signature.face.size();
			facebuffer.byteOffset = node.nvert*signature.vertex.size(); //node.getBeginOffset() +
			facebuffer.target = TINYGLTF_TARGET_ELEMENT_ARRAY_BUFFER;
			bufferViews.push_back(facebuffer);
			buffer_offset += facebuffer.byteLength;
		}

		tot_buffer_size = buffer_offset;

		//need padding
		while((buffer_offset % 4) != 0) buffer_offset++;
		int tex_offset = buffer_offset;
		int tex_index = bufferViews.size();
		if(has_textures) {
			Texture &teximage = nexus.textures[nexus.patches[node.first_patch].texture];

			tinygltf::BufferView texbuffer;
			texbuffer.buffer = buffers.size();
			texbuffer.byteLength = teximage.getSize();
			texbuffer.byteOffset = buffer_offset; //node.getBeginOffset() +
			texbuffer.target = TINYGLTF_TARGET_ELEMENT_ARRAY_BUFFER;
			bufferViews.push_back(texbuffer);
			tot_buffer_size = tex_offset + teximage.getSize();
		}

		tinygltf::Buffer buffer;
		buffer.name = "dummy";
		buffer.data.resize(node.getSize());
		memcpy(buffer.data.data(), data.memory, node.getSize());
		buffers.push_back(buffer);
		//QString uri = QString("gargo_%1.bin").arg(n);
		//buffer.uri = uri.toStdString();
		//buffers.push_back(buffer);
		//QFile file(uri);
		//file.open(QFile::WriteOnly);
		//file.write(data.memory, node.getSize());


		/* ACCESSORS */


		tinygltf::Primitive nodeprimitive;
		nodeprimitive.mode = has_index ? TINYGLTF_MODE_TRIANGLES : TINYGLTF_MODE_POINTS;
//		nodeprimitive.mode = TINYGLTF_MODE_POINTS;

		nodeprimitive.attributes["POSITION"] = accessors.size();

		vector<double> max(3, -1e20);
		vector<double> min(3, +1e20);
		vcg::Point3f *coords = (vcg::Point3f *)buffer.data.data();
		for(int i = 0; i < node.nvert; i++) {
			for(int k = 0; k < 3; k++) {
				coords[i][k] /= 100;
				double s = coords[i][k];
				max[k] = std::max(max[k], s);
				min[k] = std::min(max[k], s);
			}
		}

		tinygltf::Accessor position;
		position.bufferView = vert_index;
		position.byteOffset = 0;
		position.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
		position.count = node.nvert;
		position.type = TINYGLTF_TYPE_VEC3;
		position.minValues = min;
		position.maxValues = max;
		accessors.push_back(position);

		if(signature.vertex.hasTextures()) {
			nodeprimitive.attributes["TEXTURE_0"] = accessors.size();

			tinygltf::Accessor texture;
			texture.bufferView = vert_index;
			texture.byteOffset = (char *)data.texCoords(signature, node.nvert) - (char *)data.coords();
			texture.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
			texture.count = node.nvert;
			texture.type = TINYGLTF_TYPE_VEC2;
			accessors.push_back(texture);
		}

		if(signature.vertex.hasNormals()) {
			nodeprimitive.attributes["NORMAL"] = accessors.size();

			tinygltf::Accessor normal;
			normal.bufferView = vert_index;
			normal.byteOffset = (char *)data.normals(signature, node.nvert) - (char *)data.coords();
			normal.componentType = TINYGLTF_COMPONENT_TYPE_SHORT;
			normal.normalized = false;
			normal.count = node.nvert;
			normal.type = TINYGLTF_TYPE_VEC3;
			accessors.push_back(normal);
		}

		if(signature.vertex.hasColors()) {
			nodeprimitive.attributes["COLOR_0"] = accessors.size();

			tinygltf::Accessor color;
			color.bufferView = vert_index;
			color.byteOffset = (char *)data.colors(signature, node.nvert) - (char *)data.coords();
			color.componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE;
			//color.normalized = true;
			color.count = node.nvert;
			color.type = TINYGLTF_TYPE_VEC4;
			accessors.push_back(color);
		}


		/* MESH AND PRIMITIVE */

		int start_triangle = 0;
		for(int p = node.first_patch; p < node.last_patch(); p++) {
			Patch &patch = nexus.patches[p];

			if(signature.vertex.hasTextures())
				nodeprimitive.material = 0; //assuming 1 texture per patch
			else
				nodeprimitive.material = 0;


			tinygltf::Primitive primitive = nodeprimitive;
			primitive.indices = accessors.size();


			tinygltf::Accessor faces;
			faces.bufferView = face_index;
			faces.byteOffset = 3*start_triangle;
			faces.componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT;
			faces.count = 3*(patch.triangle_offset - start_triangle);
			start_triangle = patch.triangle_offset;

			faces.type = TINYGLTF_TYPE_SCALAR;
			accessors.push_back(faces);

			mesh.primitives.push_back(primitive);
			break;
		}

		/* MATERIALS */

		//if(!signature.vertex.hasTextures()) {
			tinygltf::Material material;

//			tinygltf::Parameter basecolor;
//			basecolor.number_array = { 1.000, 0.766, 0.336, 1.0 };
//			material.values["baseColorFactor"] = basecolor;

			tinygltf::Parameter basecolor;
			basecolor.number_value = 0;
			material.values["baseColor"] = basecolor;


			tinygltf::Parameter metallic;
			metallic.number_value = 0.5;
			metallic.has_number_value = true;
			material.values["metallicFactor"] = metallic;

			tinygltf::Parameter roughness;
			roughness.number_value = 0.1;
			roughness.has_number_value = true;
			material.values["roughnessFactor"] = roughness;

			model.materials.push_back(material);
		//} else {
			tinygltf::Image image;
			image.bufferView = tex_index;
			image.mimeType = "image/jpeg";
			model.images.push_back(image);

			tinygltf::Sampler sampler;
			sampler.minFilter = TINYGLTF_TEXTURE_FILTER_LINEAR;
			sampler.magFilter = TINYGLTF_TEXTURE_FILTER_LINEAR;
			sampler.wrapS = TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE;
			sampler.wrapT = TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE;
			model.samplers.push_back(sampler);

			tinygltf::Texture texture;
			texture.sampler = 0;
			texture.source = 0;
			model.textures.push_back(texture);
		//}


		meshes.push_back(mesh);
		tinygltf::Node tfnode;
		tfnode.mesh = 0;
		nodes.push_back(tfnode);

		tinygltf::Scene scene;
		scene.nodes.push_back(0);




		model.scenes.push_back(scene);
		model.defaultScene = 0;

		tinygltf::TinyGLTF tiny;

		std::string output = tiny.WriteGltfScene(&model,  false, false);

		//compute total size:
		while((output.size() % 4) != 0) output.push_back('\n');
		int jsonsize = output.size();

		QString uri = QString("%1_%2.glb").arg(argv[2]).arg(n);
		QFile gtb(uri);
		if(!gtb.open(QIODevice::WriteOnly)) {
			cerr << "Could not open gtb: " << argv[2] << endl;
			return 0;
		}


		uint32_t magic = 0x46546C67;
		uint32_t version = 2;
		uint32_t length = 12 + 8 + jsonsize + 8 + node.getSize();

		gtb.write((char *)&magic, 4);
		gtb.write((char *)&version, 4);
		gtb.write((char *)&length, 4);

		int chunklength = jsonsize;
		int chunktype = 0x4E4F534A;

		gtb.write((char *)&chunklength, 4);
		gtb.write((char *)&chunktype, 4);
		gtb.write((char *)output.data(), output.size());
		int pad = 0;
		gtb.write((char *)&pad, jsonsize - output.size());

		chunklength = node.getSize();
		chunktype = 0x004E4942;
		gtb.write((char *)&chunklength, 4);
		gtb.write((char *)&chunktype, 4);
		int size = node.getSize();
		gtb.write((char *)buffer.data.data(), node.getSize());

		while((size %4) != 0) {
			gtb.putChar(0);
			size++;
		}

		if(has_textures) {
			Texture &teximage = nexus.textures[nexus.patches[node.first_patch].texture];
			char *memory = (char *)nexus.file.map(teximage.getBeginOffset(), teximage.getSize());
			gtb.write(memory, teximage.getSize());
			nexus.file.unmap((unsigned char *)memory);
		}


		nexus.dropRam(n);

	}






	//Node
//	uint32_t offset;           //offset on disk (could be moved), granularity NEXUS_PADDING) //use the function
//	uint16_t nvert;
//	uint16_t nface;
//	float error;
//	nx::Cone3s cone;           //cone of normals
//	vcg::Sphere3f sphere;      //saturated sphere (extraction)
//	float tight_radius;        //tight sphere (frustum culling)
//	uint32_t first_patch;

	//Attribute
	//enum Type { NONE = 0, BYTE, UNSIGNED_BYTE, SHORT, UNSIGNED_SHORT, INT, UNSIGNED_INT, FLOAT, DOUBLE };
	//unsigned char type;
	//unsigned char number;

	//signature
	//VertexElement vertex;
	//FaceElement face;

	/* header
	uint32_t magic;
	uint32_t version;
	uint64_t nvert;            //total number of vertices (at full resolution)
	uint64_t nface;            //total number of faces
	Signature signature;
	uint32_t n_nodes;          //number of nodes in the dag
	uint32_t n_patches;        //number of links (patches) in the dag
	uint32_t n_textures;       //number of textures
	vcg::Sphere3f sphere;      //bounding sphere
	*/
	/*
	Header header;
	Node *nodes;
	Patch *patches;
	Texture *textures;

	NodeData *nodedata;
	TextureData *texturedata;
	*/

	return 1;

}

