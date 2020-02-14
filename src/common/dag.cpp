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

