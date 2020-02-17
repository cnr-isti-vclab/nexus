#include "dag.h"
#include "json.hpp"
#include <string.h>

using namespace nx;
using namespace std;
using namespace json;

template<typename T>
T bread(char *&buffer) {
	T data = *(T *)(buffer);
	buffer += sizeof(T);
	return data;
}

template<typename T>
void bwrite(char *&buffer, T value) {
	*(T *)buffer = value;
	buffer += sizeof(T);
}


//USING glTf naming, might add more :)
string components[8] = { "POSITION", "NORMAL", "COLOR_0", "UV_0", "UV_1", "WEIGHTS_0", "JOINTS_0" }; 

/* signature {
 * vertex: {
 *   POSITION: { type: int, number: int }, etc.
 * },
 * face: {
 *   INDEX: { type, number } etc.
 * },
 * flags: int
 * }
 * */

static Signature signatureFromJson(JSON json) {
	Signature sig;

	JSON vertex = json["vertex"];
	for(int i = 0; i < 8; i++) {
		if(vertex.hasKey(components[i])) {		
			auto &velement = sig.vertex.attributes[i];
			velement.type   = vertex[components[i]]["type"].ToInt();
			velement.number = vertex[components[i]]["number"].ToInt();
		}
	}	
	
	JSON face = json["face"];
	if(face.hasKey("INDEX")) {
		auto &felement = sig.face.attributes[FaceElement::INDEX];
		felement.type   = face["INDEX"]["type"].ToInt();
		felement.number = face["INDEX"]["number"].ToInt();
	}
	
	sig.flags = json["flags"].ToInt();
	return sig;
}

static JSON signatureToJson(Signature sig) {
	JSON json = json::Object();
	json["flags"] = sig.flags;
	json["vertex"] = json::Object();
	for(int i = 0; i < 8; i++) {
		Attribute &attr = sig.vertex.attributes[i];
		if(attr.type) {
			json["vertex"][components[i]]["type"] = attr.type;
			json["vertex"][components[i]]["number"] = attr.number;
		}
	}
	json["face"] = json::Object();
	Attribute &index = sig.face.attributes[FaceElement::INDEX];
	if(index.type) {
		json["face"]["INDEX"]["type"] = index.type;
		json["face"]["INDEX"]["number"] = index.number;
	}
	return json;
}


int Header3::read(char *buffer, int length) {
	if(length < 12) return 88;
	char *start = buffer;
	magic = bread<uint32_t>(buffer);
	if(magic != 0x4E787320)
		throw "Could not read header, probably not a nexus file.";
	version =  bread<uint32_t>(buffer);
	
	switch(version) {
	case 2:
		if(length < 88) 
			return 88;
		
		nvert = bread<uint64_t>(buffer);
		nface = bread<uint64_t>(buffer);
		signature = bread<Signature>(buffer);
		n_nodes = bread<uint32_t>(buffer);
		n_patches = bread<uint32_t>(buffer);
		n_textures = bread<uint32_t>(buffer);
		sphere = bread<vcg::Sphere3f>(buffer);
		index_offset = buffer - start;
		index_length = n_nodes*44 + n_patches*12 + n_textures*68;
		return 0;
		break;
		
	case 3: {

		json_length = bread<uint32_t>(buffer);
		
		if(length < 12 + json_length)
			return 12 + json_length;
		
		JSON obj = JSON::Load( buffer );
		nvert      = obj["nvert"].ToInt();
		nface      = obj["nface"].ToInt();
		signature  = signatureFromJson(obj["signature"]);
		n_nodes    = obj["n_nodes"].ToInt();
		n_patches  = obj["n_patches"].ToInt();
		n_textures = obj["n_textures"].ToInt();
		JSON jsphere = obj["sphere"];
		sphere.Radius() = jsphere["radius"].ToFloat();
		sphere.Center()[0] = jsphere["center"][0].ToFloat();
		sphere.Center()[1] = jsphere["center"][1].ToFloat();
		sphere.Center()[2] = jsphere["center"][2].ToFloat();
		index_offset = json_length + 12;
		index_length = n_nodes*44 + n_patches*12 + n_textures*68;
		return 0;
		}
		break;

	default: 
		throw "Unsupported nexus version.";
	}
}


std::vector<char> Header3::write() {
	
	//vector<char> buffer(12);
	JSON json = json::Object();
	json["signature"] = signatureToJson(signature);

	json["nvert"]      = nvert;
	json["nface"]      = nface;
	json["n_nodes"]    = n_nodes;
	json["n_patches"]  = n_patches;
	json["n_textures"] = n_textures;
	
	JSON jsphere = json::Object();
	jsphere["radius"] = sphere.Radius();
	jsphere["center"] = json::Array(sphere.Center()[0],sphere.Center()[1], sphere.Center()[2]);
	json["sphere"] = jsphere;
			
	string str = json.dump();
	json_length = str.size() + 1;
	while((json_length % 4) != 0)
		json_length++;
	
	std::vector<char> buffer(json_length + 12);
	char *data = buffer.data();
	bwrite<uint32_t>(data, magic);
	bwrite<uint32_t>(data, version);
	bwrite<uint32_t>(data, json_length);
	assert(strlen(str.c_str()) == str.size());
	memcpy(data, str.c_str(), str.size()+1);
	
	index_offset = json_length + 12;
	index_length = n_nodes*44 + n_patches*12 + n_textures*68;
	return buffer;
}


/*
{
	pbrMetallicRoughness: {
		baseColorTexture: { index: 1, texCoord: 1 }
		baseColorFactor: [ 1, 0, 1, 1],
		metallicRoughnessTexture: { index: 1, texCoord: 1 }
		metallicFactor: 1.0,
		roughnessFactor: 1.0,
	},
	specular: [1, 1, 1, 1],
	glossines: [1, 1, 1, 1],
	normalTexture: { scale:0.9, index: 1, texCoord: 1 },
	//bump, specular, glossines, mr, ao

}
*/



int readMaterial(JSON &json) {
	Material m;
	JSON pbr = json["pbrMetallicRoughness"];

	//BASECOLOR
	if(pbr.hasKey("baseColorTexture"))
		m.color_map = pbr["baseColorTexture"]["index"].ToInt();
	if(pbr.hasKey("baseColorFactor")) {
		JSON cf = pbr["baseColorFactor"];
		for(int i = 0; i < 4; i++)
			m.color[i] = cf[i].ToFloat()*255;
	}

	//METALLIC
	if(pbr.hasKey("metallicRoughnessTexture"))
		m.metallic_map = pbr["metallicRoughnessTexture"]["index"].ToInt();

	if(pbr.hasKey("metallicFactor"))
		m.metallic = pbr["metallicFactor"].ToFloat()*255;

	if(json.hasKey("roughnessFactor"))
		m.roughness = pbr["roughnessFactor"].ToFloat()*255;

	//NORMAL
	if(json.hasKey("normalTexture")) {
		m.norm_map=  json["normalTexture"]["index"].ToInt();
		m.norm_scale=  json["normalTexture"]["scale"].ToFloat()/255;
	}

	if(json.hasKey("bumpTexture")) {
		m.norm_map=  json["bumpTexture"]["index"].ToInt();
	}

	//SPECULAR
	if(json.hasKey("specularFactor")) {
		JSON s = pbr["specularFactor"];
		for(int i = 0; i < 4; i++)
			m.specular[i] = s[i].ToFloat()*255;
	}
	if(json.hasKey("specularTexture"))
		m.glossines = pbr["specularTexture"].ToInt();

	//GLOSSINESS
	if(json.hasKey("glossinesFactor"))
		m.glossines = pbr["glossinesFactor"].ToFloat()*255;

	if(json.hasKey("glossinesTexture"))
		m.glossines = pbr["glossinesTexture"]["index"].ToInt();


	//OCCLUSION
	if(json.hasKey("occlusionFactor"))

	if(json.hasKey("occlusionTexture")) {
		m.glossines_map = pbr["occlusionTexture"]["index"].ToInt();
		m.glossines = pbr["occlusionFactor"]["strength"].ToFloat()*255;
	}
}

JSON writeMAterial(Material &m) {
	JSON json = json::Object();
	if(m.color[3] != 0 || m.color_map != -1 || m.metallic != 0 || m.roughness != 0 || m.metallic_map != 0) {
		JSON pbr = json::Object();
		if(m.color[3] != 0)
			pbr["baseColorFactor"] = json::Array(m.color[0]/225.f, m.color[1]/225.f, m.color[2]/225.f, m.color[3]/255.f);
		if(m.color_map != -1)
			pbr["baseColorTexture"] = { "index", m.color_map };

		if(m.metallic != 0)
			pbr["metallicFactor"] = m.metallic/255.f;
		if(m.roughness != 0)
			pbr["roughnessFactor"] = m.metallic/255.f;
		if(m.metallic_map != -1)
			pbr["metallicRoughnessTexture"] = { "index", m.metallic_map };
		\
		json["pbrMetallicRoughness"] = pbr;
	}
	if(m.norm_map != -1)
		json["normalTexture"] = {
			"scale", m.norm_scale/255.f,
			"index", m.norm_map,
			};

	if(m.specular_map != -1)
		json["specularTexture"] = { "index", m.norm_map };
	if(m.specular[4] != 0)
		json["specularFactor"] = json::Array(m.specular[0]/255.f, m.specular[1]/255.f, m.specular[2]/255.f, m.specular[3]/255.f);


	if(m.glossines_map  != -1)
		json["glossinessTexture"] = { "index", m.norm_map };
	if(m.glossines != 0)
		json["glossinesFactor"] = m.glossines/255.f;

	if(m.occlusion_map >= 0)
		json["occlusionTexture"] = {
			"strength", m.occlusion/255.f,
			"index", m.occlusion_map,
			};

	return json;
}


