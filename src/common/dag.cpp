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
			velement.type   = uint8_t(vertex[components[i]]["type"].ToInt());
			velement.number = uint8_t(vertex[components[i]]["number"].ToInt());
		}
	}	
	
	JSON face = json["face"];
	if(face.hasKey("INDEX")) {
		auto &felement = sig.face.attributes[FaceElement::INDEX];
		felement.type   = uint8_t(face["INDEX"]["type"].ToInt());
		felement.number = uint8_t(face["INDEX"]["number"].ToInt());
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


Material readMaterial(JSON &json);

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
		sphere.Radius() = float(jsphere["radius"].ToFloat());
		sphere.Center()[0] = float(jsphere["center"][0].ToFloat());
		sphere.Center()[1] = float(jsphere["center"][1].ToFloat());
		sphere.Center()[2] = float(jsphere["center"][2].ToFloat());

		//materials
		JSON mats = obj["materials"];
		for(int i = 0; i < mats.length(); i++) {
			JSON &mat = mats.at(i);
			Material m = readMaterial(mat);
			materials.push_back(m);
		}

		index_offset = json_length + 12;
		index_length = n_nodes*44 + n_patches*12 + n_textures*68;

		}
		return 0;
	default: 
		throw "Unsupported nexus version.";
	}
}


JSON writeMaterial(Material &m);

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

	JSON mats = json::Array();
	for(Material &m: materials)
		mats.append(writeMaterial(m));
	json["materials"] = mats;
			
	string str = json.dump();
	json_length = str.size() ;
	while((json_length % 4) != 0) {
		json_length++;
		str.push_back(' ');
	}
	
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



Material readMaterial(JSON &json) {
	Material m;
	JSON pbr = json["pbrMetallicRoughness"];

	//BASECOLOR
	if(pbr.hasKey("baseColorTexture"))
		m.color_map = pbr["baseColorTexture"]["index"].ToInt();
	if(pbr.hasKey("baseColorFactor")) {
		JSON cf = pbr["baseColorFactor"];
		for(int i = 0; i < 4; i++)
			m.color[i] = float(cf[i].ToFloat());
	}

	//METALLIC
	if(pbr.hasKey("metallicRoughnessTexture"))
		m.metallic_map = int32_t(pbr["metallicRoughnessTexture"]["index"].ToInt());

	if(pbr.hasKey("metallicFactor"))
		m.metallic = float(pbr["metallicFactor"].ToFloat());

	if(json.hasKey("roughnessFactor"))
		m.roughness = float(pbr["roughnessFactor"].ToFloat());

	//NORMAL
	if(json.hasKey("normalTexture")) {
		m.norm_map=  int32_t(json["normalTexture"]["index"].ToInt());
		m.norm_scale=  float(json["normalTexture"]["scale"].ToFloat());
	}

	if(json.hasKey("bumpTexture")) {
		m.norm_map=  json["bumpTexture"]["index"].ToInt();
	}

	//SPECULAR
	if(json.hasKey("specularFactor")) {
		JSON s = pbr["specularFactor"];
		for(int i = 0; i < 4; i++)
			m.specular[i] = float(s[i].ToFloat());
	}
	if(json.hasKey("specularTexture"))
		m.specular_map = int32_t(pbr["specularTexture"].ToInt());

	//GLOSSINESS
	if(json.hasKey("glossinesFactor"))
		m.glossines = float(pbr["glossinesFactor"].ToFloat());

	if(json.hasKey("glossinesTexture"))
		m.glossines_map = int32_t(pbr["glossinesTexture"]["index"].ToInt());


	//OCCLUSION
	if(json.hasKey("occlusionFactor"))

	if(json.hasKey("occlusionTexture")) {
		m.glossines_map = int32_t(pbr["occlusionTexture"]["index"].ToInt());
		m.glossines = float(pbr["occlusionFactor"]["strength"].ToFloat());
	}
	return m;
}

JSON writeMaterial(Material &m) {
	JSON json = json::Object();
	if(m.color[3] != 0.0f || m.color_map != -1 || m.metallic != 0.0f || m.roughness != 0.0f || m.metallic_map != 0) {
		JSON pbr = json::Object();
		if(m.color[3] != 0.0f)
			pbr["baseColorFactor"] = json::Array(m.color[0], m.color[1], m.color[2], m.color[3]);
		if(m.color_map != -1)
			pbr["baseColorTexture"] = { "index", m.color_map };

		if(m.metallic != 0.0f)
			pbr["metallicFactor"] = m.metallic;
		if(m.roughness != 0.0f)
			pbr["roughnessFactor"] = m.metallic;
		if(m.metallic_map != -1)
			pbr["metallicRoughnessTexture"] = { "index", m.metallic_map };
		\
		json["pbrMetallicRoughness"] = pbr;
	}
	if(m.norm_map != -1)
		json["normalTexture"] = {
			"scale", m.norm_scale,
			"index", m.norm_map,
			};

	if(m.specular_map != -1)
		json["specularTexture"] = { "index", m.norm_map };
	if(m.specular[3] != 0.0f)
		json["specularFactor"] = json::Array(m.specular[0], m.specular[1], m.specular[2], m.specular[3]);


	if(m.glossines_map  != -1)
		json["glossinessTexture"] = { "index", m.norm_map };
	if(m.glossines != 0.0f)
		json["glossinesFactor"] = m.glossines;

	if(m.occlusion_map >= 0)
		json["occlusionTexture"] = {
			"strength", m.occlusion,
			"index", m.occlusion_map,
			};

	return json;
}


