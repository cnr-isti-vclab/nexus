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
#define _FILE_OFFSET_BITS 64

#include "nexusdata.h"
#include "qtnexusfile.h"
//#include <vcg/space/line3.h>
//#include <vcg/space/intersection3.h>
//#include <vcg/space/sphere3.h>

#include <corto/decoder.h>

//#if _MSC_VER >= 1800
#include <random>
//#endif

#include <iostream>
#include <stdexcept>
using namespace std;
//using namespace vcg;
using namespace nx;

namespace {

void validateHeader(const nx::Header &header) {
	if(header.magic != NEXUS_FORMAT_MAGIC &&
	   header.magic != NEXUS_PREVIOUS_FORMAT_MAGIC &&
	   header.magic != NEXUS_LEGACY_MAGIC)
		throw std::runtime_error("could not read header, probably not a nexus file");
	if(header.version < NEXUS_LEGACY_FORMAT_VERSION)
		throw std::runtime_error("Nexus file format version " + std::to_string(header.version) +
		                         " is older than the supported version " +
		                         std::to_string(NEXUS_LEGACY_FORMAT_VERSION));
	if(header.version > NEXUS_FORMAT_VERSION)
		throw std::runtime_error("Nexus file format version " + std::to_string(header.version) +
		                         " is newer than the supported version " +
		                         std::to_string(NEXUS_FORMAT_VERSION));
	const uint32_t expected_magic = header.version == NEXUS_FORMAT_VERSION
		? NEXUS_FORMAT_MAGIC
		: header.version == NEXUS_PREVIOUS_FORMAT_VERSION
			? NEXUS_PREVIOUS_FORMAT_MAGIC
			: NEXUS_LEGACY_MAGIC;
	if(header.magic != expected_magic)
		throw std::runtime_error("Nexus file magic does not match its format version");
}

} // namespace

uint16_t *NodeData::faces(Signature &sig, uint32_t nvert, char *mem) {
	return (uint16_t  *)(mem + nvert*sig.vertex.vertexSize());
}


NexusData::NexusData(): nodes(0), patches(0), textures(0), nodedata(0), texturedata(0), nroots(0) {
	file = new QTNexusFile();
}

NexusData::~NexusData() {
	close();
	delete file;
}

bool NexusData::open(const char *_uri) {

	file->setFileName(_uri);
	if(!file->open(NexusFile::Read))
		//file = fopen(_uri, "rb+");
		//if(!file)
		return false;
	loadHeader();
	loadIndex();
	return true;
}

void NexusData::close() {
	flush();
}

void NexusData::flush() {
	if(nodes && nodedata) {
		for(unsigned int i = 0; i < header.n_nodes; i++)
			if(nodedata[i].memory)
				dropRam(i);
	}
	if(texturedata) {
		for(unsigned int i = 0; i < header.n_textures; i++) {
			TextureData &data = texturedata[i];
			if(!data.memory)
				continue;
			if(data.memory_owned)
				delete []data.memory;
			else if(header.signature.isDeepzoom())
				file->dropDZTex(data.memory);
			else
				file->unmap((unsigned char *)data.memory);
			data.memory = NULL;
			data.memory_owned = false;
		}
	}
	delete []nodes;
	nodes = NULL;
	delete []patches;
	patches = NULL;
	delete []textures;
	textures = NULL;
	delete []nodedata;
	nodedata = NULL;
	delete []texturedata;
	texturedata = NULL;
}

void NexusData::loadHeader() {
	char buffer[Header::serializedSize()];
	const int readed = file->read(buffer, Header::serializedSize());
	if(readed != static_cast<int>(Header::serializedSize()))
		throw std::runtime_error("could not read header, file too short");
	header.deserialize(buffer);
	validateHeader(header);
}

void NexusData::loadHeader(char *buffer) {
	header.deserialize(buffer);
	validateHeader(header);
}

void NexusData::countRoots() {
	//find number of roots:
	nroots = header.n_nodes;
	for(uint32_t j = 0; j < nroots; j++) {
		for(uint32_t i = nodes[j].first_patch; i < nodes[j].last_patch(); i++)
			if(patches[i].node < nroots)
				nroots = patches[i].node;
	}
}

uint64_t NexusData::indexSize() {
	return header.n_nodes * sizeof(Node) +
			header.n_patches * sizeof(Patch) +
			header.n_textures * sizeof(Texture);
}

void NexusData::initIndex() {
	nodes = new Node[header.n_nodes];
	patches = new Patch[header.n_patches];
	textures = new Texture[header.n_textures];

	nodedata = new NodeData[header.n_nodes];
	texturedata = new TextureData[header.n_textures];
}

void NexusData::loadIndex() {
	initIndex();

	//fread(nodes, sizeof(Node), header.n_nodes, file);
	//fread(patches, sizeof(Patch), header.n_patches, file);
	//fread(textures, sizeof(Texture), header.n_textures, file);
	file->read((char *)nodes, sizeof(Node)*header.n_nodes);
	file->read((char *)patches, sizeof(Patch)*header.n_patches);
	file->read((char *)textures, sizeof(Texture)*header.n_textures);
	countRoots();
}

void NexusData::loadIndex(char *buffer) {
	initIndex();

	uint32_t size = sizeof(Node)*header.n_nodes;
	memcpy(nodes, buffer, size);
	buffer += size;

	size = sizeof(Patch)*header.n_patches;
	memcpy(patches, buffer, size);
	buffer += size;

	size = sizeof(Texture)*header.n_textures;
	memcpy(textures, buffer, size);

	countRoots();
}

uint32_t NexusData::size(uint32_t node) {
	return nodes[node].getSize();
}


uint64_t NexusData::loadRam(uint32_t n) {

	Signature &sign = header.signature;
	if(sign.flags & Signature::MECO)
		throw "MECO-compressed Nexus files are no longer supported";
	Node &node = nodes[n];
	uint64_t offset = node.getBeginOffset();

	NodeData &d = nodedata[n];
	uint64_t compressed_size = node.getEndOffset() - offset;

	const uint64_t interleaved_vertex_size =
		static_cast<uint64_t>(node.nvert) * sign.vertex.vertexSize();
	const uint64_t planar_vertex_size =
		static_cast<uint64_t>(node.nvert) * sign.vertex.size();
	const uint64_t face_size = static_cast<uint64_t>(node.nface) * sign.face.size();
	uint64_t size = interleaved_vertex_size + face_size;
	const uint64_t source_size =
		(sign.isInterleaved() ? interleaved_vertex_size : planar_vertex_size) + face_size;



	if(!sign.isCompressed()) {
		char *source;
		if(sign.isDeepzoom()) {
			std::lock_guard<std::mutex> file_locker(file_mutex);
			source = file->loadDZNode(n);
		} else {
			std::lock_guard<std::mutex> file_locker(file_mutex);
			source = (char *)file->map(offset, source_size);
		}
		if(!source)
			throw std::runtime_error("could not load node data");

		if(sign.isInterleaved()) {
			d.memory = source;
			d.memory_owned = false;
		} else {
			PlanarVertexData planar(sign.vertex, node.nvert);
			planar.readPlanar(source);
			d.memory = new char[size]{};
			d.memory_owned = true;
			planar.interleave(d.memory);
			if(face_size)
				memcpy(d.memory + interleaved_vertex_size, source + planar_vertex_size, face_size);

			std::lock_guard<std::mutex> file_locker(file_mutex);
			if(sign.isDeepzoom())
				file->dropDZNode(source);
			else
				file->unmap((unsigned char *)source);
		}

	} else {

		char *buffer = nullptr;
		if(sign.isDeepzoom()) {
			std::lock_guard<std::mutex> file_locker(file_mutex);
			buffer = file->loadDZNode(n);
		} else {
			buffer = new char[compressed_size];
			std::lock_guard<std::mutex> file_locker(file_mutex);
			file->seek(offset);
			int64_t r = file->read(buffer, compressed_size);
			assert(r == (int64_t)compressed_size);
		}

		d.memory = new char[size]{};
		d.memory_owned = true;

		if(sign.flags & Signature::CORTO) {
			PlanarVertexData planar(sign.vertex, node.nvert);

			crt::Decoder decoder(compressed_size, (unsigned char *)buffer);

			decoder.setPositions(reinterpret_cast<float *>(planar.component(VertexElement::COORD)));
			if(sign.vertex.hasNormals())
				decoder.setNormals(reinterpret_cast<int16_t *>(planar.component(VertexElement::NORM)));
			if(sign.vertex.hasColors())
				decoder.setColors(reinterpret_cast<unsigned char *>(planar.component(VertexElement::COLOR)));
			if(sign.vertex.hasTextures())
				decoder.setUvs(reinterpret_cast<float *>(planar.component(VertexElement::TEX)));
			if(node.nface)
				decoder.setIndex(d.faces(sign, node.nvert));

			decoder.decode();

			//Shuffle every attribute with the point order used by CORTO.
			if(!sign.face.hasIndex()) {
				std::vector<int> order(node.nvert);
				for(int i = 0; i < node.nvert; i++)
					order[i] = i;

				unsigned seed = 0;
				std::shuffle(order.begin(), order.end(), std::default_random_engine(seed));

				for(unsigned int c = 0; c < VertexElement::COMPONENT_COUNT; ++c) {
					const std::size_t bytes = sign.vertex.attributes[c].size();
					if(!bytes)
						continue;
					std::vector<char> shuffled(static_cast<std::size_t>(node.nvert) * bytes);
					char *component = planar.component(c);
					for(uint32_t i = 0; i < node.nvert; ++i)
						memcpy(shuffled.data() + static_cast<std::size_t>(i) * bytes,
						       component + static_cast<std::size_t>(order[i]) * bytes,
						       bytes);
					memcpy(component, shuffled.data(), shuffled.size());
				}
			}

			planar.interleave(d.memory);
		}
		if(sign.isDeepzoom()) {
			std::lock_guard<std::mutex> file_locker(file_mutex);
			file->dropDZNode(buffer);
		} else {
			delete []buffer;
		}
	}

	assert(d.memory);
	// If DEEPZOOM mode, textures are stored externally (in _files/) — skip texture loading here.
	if(header.n_textures) {
		//be sure to load images
		for(uint32_t p = node.first_patch; p < node.last_patch(); p++) {
			uint32_t t = patches[p].texture;
			if(t == 0xffffffff) continue;

			TextureData &data = texturedata[t];
			std::lock_guard<std::mutex> texture_locker(data.mutex);
			data.count_ram++;
			if(data.count_ram > 1)
				continue;

			Texture &texture = textures[t];
			if(sign.isDeepzoom()) {
				std::lock_guard<std::mutex> file_locker(file_mutex);
				data.memory = file->loadDZTex(n);
			} else {
				std::lock_guard<std::mutex> file_locker(file_mutex);
				data.memory = (char *)file->map(texture.getBeginOffset(), texture.getSize());
			}
			data.memory_owned = false;
			if(!data.memory) {
				cerr << "Failed mapping texture data" << endl;
				exit(0);
			}

			loadImageFromData(data, t);

			/*
			QImage img;
			bool success = img.loadFromData((uchar *)data.memory, texture.getSize());
			file->unmap((uchar *)data.memory);

			if(!success) {
				cerr << "Failed loading texture" << endl;
				exit(0);
			}

			img = img.convertToFormat(QImage::Format_RGBA8888);
			data.width = img.width();
			data.height = img.height();

			int imgsize = data.width*data.height*4;
			data.memory = new char[imgsize];
			data.memory_owned = true;

			//flip memory for texture
			int linesize = img.width()*4;
			char *mem = data.memory + linesize*(img.height()-1);
			for(int i = 0; i < img.height(); i++) {
				memcpy(mem, img.scanLine(i), linesize);
				mem -= linesize;
			}
			*/
			int imgsize = data.width * data.height * 4;
			size += imgsize;
		}
	}
	return size;
}

uint64_t NexusData::dropRam(uint32_t n, bool write) {
	Node &node = nodes[n];
	NodeData &data = nodedata[n];

	assert(!write);
	assert(!data.vbo);
	assert(data.memory);

	auto &sign = header.signature;
	if(data.memory_owned) {
		delete []data.memory;
	} else if(sign.isDeepzoom()) {
		std::lock_guard<std::mutex> file_locker(file_mutex);
		file->dropDZNode(data.memory);
	} else {
		std::lock_guard<std::mutex> file_locker(file_mutex);
		file->unmap((unsigned char *)data.memory);
	}

	data.memory = NULL;
	data.memory_owned = false;

	//report RAM size for compressed meshes.
	uint32_t size = node.nvert * sign.vertex.vertexSize() +
				node.nface * sign.face.size();

	if(header.n_textures) {
		for(uint32_t p = node.first_patch; p < node.last_patch(); p++) {
			uint32_t t = patches[p].texture;
			if(t == 0xffffffff) continue;

			TextureData &tdata = texturedata[t];
			std::lock_guard<std::mutex> texture_locker(tdata.mutex);
			tdata.count_ram--;
			if(tdata.count_ram != 0) continue;
			if(tdata.memory_owned) {
				delete []tdata.memory;
			} else if(sign.isDeepzoom()) {
				std::lock_guard<std::mutex> file_locker(file_mutex);
				file->dropDZTex(tdata.memory);
			} else {
				std::lock_guard<std::mutex> file_locker(file_mutex);
				file->unmap((unsigned char *)tdata.memory);
			}

			tdata.memory = NULL;
			tdata.memory_owned = false;
			size += tdata.width*tdata.height*4;
		}
	}
	return size;
}


//local structure for keeping track of nodes and their distance
class DNode {
public:
	uint32_t node;
	float dist;
	DNode(uint32_t n = 0, float d = 0.0f): node(n), dist(d) {}
	bool operator<(const DNode &n) const {
		return dist < n.dist;
	}
};

bool closest(const nx::Sphere &sphere, const nx::Ray3 &ray, float &distance) {
	if(sphere.Radius() < 0) return false;
	nx::Vector3f offset = ray.Origin() - sphere.Center();
	const nx::Vector3f &direction = ray.Direction();
	float b = offset.dot(direction);
	float c = offset.squaredNorm() - sphere.Radius()*sphere.Radius();
	float discriminant = b*b - c; // direction is normalized
	if(discriminant < 0) return false;
	float root = std::sqrt(discriminant);
	float near_distance = -b - root;
	float far_distance = -b + root;
	if(far_distance < 0) return false;
	distance = near_distance < 0 ? 0.0f : near_distance;
	return true;
}

bool NexusData::intersects(nx::Ray3 &ray, float &distance) {
	uint32_t sink = header.n_nodes -1;

	if(!ray.NormalizeDirection()) return false;

	if(!closest(header.sphere, ray, distance))
		return false;

	distance = 1e20f;

	//keep track of visited nodes (it is a DAG not a tree!
	std::vector<bool> visited(header.n_nodes, false);
	std::vector<DNode> candidates;      //heap of nodes
	candidates.push_back(DNode(0, distance));

	bool hit = false;

	while(candidates.size()) {
		pop_heap(candidates.begin(), candidates.end());
		DNode candidate = candidates.back();
		candidates.pop_back();

		if(candidate.dist > distance) break;

		Node &node = nodes[candidate.node];

		if(node.first_patch  == sink) {
			if(candidate.dist < distance) {
				distance = candidate.dist;
				hit = true;
			}
			continue;
		}

		//not a leaf, insert children (if they intersect
		for(uint32_t i = node.first_patch; i < node.last_patch(); i++) {
			uint32_t child = patches[i].node;
			if(child == sink) continue;
			if(visited[child]) continue;

			float d;
			if(closest(nodes[child].sphere, ray, d)) {
				candidates.push_back(DNode(child, d));
				push_heap(candidates.begin(), candidates.end());
				visited[child] = true;
			}
		}
	}
	distance = sqrt(distance);
	return hit;
}

