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
#include <QTextStream>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <vector>
#include "extractor.h"

#include <corto/corto.h>

#include <vcg/space/triangle3.h>
using namespace std;
using namespace nx;

namespace {
QString deepzoomFolderFromOutput(const QString &output) {
	QFileInfo info(output);
	QString dirPath = info.absolutePath();
	if(dirPath.isEmpty())
		dirPath = ".";
	return dirPath + "/" + info.completeBaseName() + "_files";
}

QString deepzoomFilePath(const QString &folder, uint32_t index, const QString &ext) {
	return QString("%1/%2.%3").arg(folder).arg(index).arg(ext);
}

void ensureDeepzoomFolder(const QString &folder) {
	QDir dir;
	if(dir.exists(folder))
		return;
	if(!dir.mkpath(folder))
		throw QString("could not create deepzoom folder %1").arg(folder);
}
}

Extractor::Extractor(NexusData *nx):
	transform(false),
	max_size(0), current_size(0),
	min_error(0),current_error(0),
	max_triangles(0), current_triangles(0) {
	
	nexus = nx;
	selected.resize(nexus->header.n_nodes, true);
	selected.back() = false;
}

int Extractor::sinkDistance(int node) {
	int dist = 0;
	int sink = nexus->header.n_nodes - 1;
	while(node != sink) {
		Node &nodeData = nexus->nodes[node];
		node = nexus->patches[nodeData.first_patch].node;
		dist++;
	}
	return dist;

}
int Extractor::levelCount() {
	return sinkDistance(0);
}

void Extractor::setMatrix(vcg::Matrix44f m) {
	transform = true;
	matrix = m;
}

void Extractor::selectBySize(quint64 size) {
	max_size = size;
	traverse(nexus);
}

void Extractor::selectByError(float error) {
	min_error = error;
	traverse(nexus);
}

void Extractor::selectByTriangles(quint64 triangles) {
	max_triangles = triangles;
	traverse(nexus);
}

void Extractor::selectByLevel(int level) {
	nlevels = levelCount();
	if(level >= nlevels)
		throw QString("Extracted level must be < than the number of levels: %1").arg(nlevels);
	max_level = level;
	traverse(nexus);
} //select up to level included (zero based)

void Extractor::dropLevel() {
	selected.resize(nexus->header.n_nodes, true);
	quint32 n_nodes = nexus->header.n_nodes;
	quint32 sink = n_nodes-1;
	//unselect nodes which output is the sink (definition of last level :)
	for(uint i = 0; i < n_nodes-1; i++) {
		nx::Node &node = nexus->nodes[i];
		if(nexus->patches[node.first_patch].node == sink)
			selected[i] = false;
		
	}
	selected.back() = false; //sink unselection purely for coherence
}

void Extractor::save(QString output, nx::Signature &signature, bool legacy) {
	QFile file;
	file.setFileName(output);
	if(!file.open(QIODevice::WriteOnly | QFile::Truncate))
		throw QString("could not open file " + output + " for writing");

	const bool saveAsDeepzoom = signature.isDeepzoom();
	QString deepzoomFolder;
	if(saveAsDeepzoom) {
		deepzoomFolder = deepzoomFolderFromOutput(output);
		ensureDeepzoomFolder(deepzoomFolder);
	}
	
	nx::Header header = nexus->header;
	header.signature = signature;
	header.magic = legacy ? nx::NEXUS_LEGACY_MAGIC : nx::NEXUS_FORMAT_MAGIC;
	header.version = legacy ? nx::NEXUS_LEGACY_FORMAT_VERSION : nx::NEXUS_FORMAT_VERSION;
	header.nvert = 0;
	header.nface = 0;
	
	if(transform) {
		nx::Vector3f &center = header.sphere.Center();
		vcg::Point3f transformed = matrix * vcg::Point3f(center.x, center.y, center.z);
		center = {transformed[0], transformed[1], transformed[2]};
	}
	
	vector<nx::Node> nodes;
	vector<nx::Patch> patches;
	vector<nx::Texture> textures;
	
	uint n_nodes = nexus->header.n_nodes;
	vector<int> node_remap(n_nodes, -1);
	
	
	for(uint i = 0; i < n_nodes-1; i++) {
		if(!selected[i]) continue;
		nx::Node node = nexus->nodes[i];
		if(transform) {
			nx::Vector3f &center = node.sphere.Center();
			vcg::Point3f transformed = matrix * vcg::Point3f(center.x, center.y, center.z);
			center = {transformed[0], transformed[1], transformed[2]};
		}
		header.nvert += node.nvert;
		header.nface += node.nface;
		
		node_remap[i] = nodes.size();
		
		node.first_patch = patches.size();
		for(uint k = nexus->nodes[i].first_patch; k < nexus->nodes[i].last_patch(); k++) {
			nx::Patch patch = nexus->patches[k];
			patches.push_back(patch);
		}
		
		nodes.push_back(node);
	}
	
	nx::Node sink = nexus->nodes[n_nodes -1];
	sink.first_patch = patches.size();
	nodes.push_back(sink);
	
	for(uint i = 0; i < patches.size(); i++) {
		nx::Patch &patch = patches[i];
		int remapped = node_remap[patch.node];
		if(remapped == -1)
			patch.node = nodes.size()-1;
		else
			patch.node = remapped;
		assert(patch.node < nodes.size());
	}
	
	cout << "Textures: " << nexus->header.n_textures << endl;
	textures.resize(nexus->header.n_textures);
	
	header.n_nodes = nodes.size();
	header.n_patches = patches.size();
	header.n_textures = textures.size();
	
	
	const quint64 header_size = nx::Header::serializedSize();
	quint64 size = header_size +
			nodes.size()*sizeof(nx::Node) +
			patches.size()*sizeof(nx::Patch) +
			textures.size()*sizeof(nx::Texture);
	size = pad(size);
	
	for(uint i = 0; i < nodes.size(); i++) {
		nodes[i].offset += size/NEXUS_PADDING;
	}
	
	//TODO should actually remove textures not used anymore.
	
	char serialized_header[nx::Header::serializedSize()];
	header.serialize(serialized_header);
	file.write(serialized_header, sizeof(serialized_header));
	file.write((char *)&*nodes.begin(), sizeof(nx::Node)*nodes.size());
	file.write((char *)&*patches.begin(), sizeof(nx::Patch)*patches.size());
	file.write((char *)&*textures.begin(), sizeof(nx::Texture)*textures.size());
	file.seek(size);
	
	for(uint i = 0; i < node_remap.size()-1; i++) {
		int n = node_remap[i];
		if(n == -1) continue;
		nx::Node &node = nodes[n];
		node.offset = file.pos()/NEXUS_PADDING;
		
		nexus->loadRam(i);
		
		NodeData &data = nexus->nodedata[i];
		char *memory = data.memory;
		const quint64 data_size = nexus->nodes[i].getSize();
		std::unique_ptr<char[]> transformed_memory;
		if(transform) {
			transformed_memory = std::make_unique<char[]>(data_size);
			memory = transformed_memory.get();
			memcpy(memory, data.memory, data_size);
			
			const size_t stride = signature.vertex.size();
			const size_t coord_offset = signature.vertex.componentOffset(nx::VertexElement::COORD);
			for(uint32_t k = 0; k < node.nvert; k++) {
				nx::Vector3f &v = *reinterpret_cast<nx::Vector3f *>(
					memory + k*stride + coord_offset);
				vcg::Point3f transformed = matrix * vcg::Point3f(v.x, v.y, v.z);
				v = {transformed[0], transformed[1], transformed[2]};
			}
		}
		if(signature.isCompressed()) {
			if(saveAsDeepzoom) {
				QFile nodeFile(deepzoomFilePath(deepzoomFolder, n, QStringLiteral("nxn")));
				if(!nodeFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
					throw QString("could not open node file %1 for writing").arg(nodeFile.fileName());
				compress(nodeFile, signature, node, nexus->nodedata[i], memory, &*patches.begin());
				nodeFile.close();
			} else {
				compress(file, signature, node, nexus->nodedata[i], memory, &*patches.begin());
			}
		} else if(legacy) {
			nx::PlanarVertexData planar(signature.vertex, node.nvert);
			planar.deinterleave(memory);
			const quint64 vertex_bytes =
				static_cast<const nx::Element &>(signature.vertex).size() * node.nvert;
			const quint64 face_bytes = signature.face.size() * node.nface;
			const quint64 geometry_bytes = vertex_bytes + face_bytes;
			const quint32 output_size = geometry_bytes ? pad(static_cast<quint32>(geometry_bytes)) : 0;
			vector<char> legacy_memory(output_size, 0);

			const unsigned int component_order[nx::VertexElement::COMPONENT_COUNT] = {
				nx::VertexElement::COORD, nx::VertexElement::TEX,
				nx::VertexElement::NORM, nx::VertexElement::COLOR,
				nx::VertexElement::DATA0, nx::VertexElement::DATA0 + 1,
				nx::VertexElement::DATA0 + 2, nx::VertexElement::DATA0 + 3
			};
			quint64 planar_offset = 0;
			for(unsigned int component : component_order) {
				const quint64 component_size = signature.vertex.attributes[component].size();
				if(!component_size)
					continue;
				const char *component_data = planar.component(component);
				if(!component_data)
					throw std::runtime_error("missing planar vertex component for legacy output");
				const quint64 bytes = component_size * node.nvert;
				memcpy(legacy_memory.data() + planar_offset, component_data, bytes);
				planar_offset += bytes;
			}
			if(planar_offset != vertex_bytes)
				throw std::runtime_error("unsupported legacy vertex layout");
			if(face_bytes) {
				const char *faces = reinterpret_cast<const char *>(
					NodeData::faces(signature, node.nvert, memory));
				memcpy(legacy_memory.data() + vertex_bytes, faces, face_bytes);
			}

			if(saveAsDeepzoom) {
				QFile nodeFile(deepzoomFilePath(deepzoomFolder, n, QStringLiteral("nxn")));
				if(!nodeFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
					throw QString("could not open node file %1 for writing").arg(nodeFile.fileName());
				nodeFile.write(legacy_memory.data(), legacy_memory.size());
				nodeFile.close();
			} else {
				file.write(legacy_memory.data(), legacy_memory.size());
			}
		} else {
			if(saveAsDeepzoom) {
				QFile nodeFile(deepzoomFilePath(deepzoomFolder, n, QStringLiteral("nxn")));
				if(!nodeFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
					throw QString("could not open node file %1 for writing").arg(nodeFile.fileName());
				nodeFile.write(memory, data_size);
				nodeFile.close();
			} else {
				file.write(memory, data_size);
			}
		}
		nexus->dropRam(i);
	}
	nodes.back().offset = file.pos()/NEXUS_PADDING;
	
	//save textures
	if(textures.size()) {
		for(uint32_t i = 0; i < textures.size()-1; i++) {
			Texture &in = nexus->textures[i];
			Texture &out = textures[i] = in;
			
			quint64 start = in.getBeginOffset();
			quint64 size = in.getSize();
			char *memory = 0;
			if(nexus->header.signature.isDeepzoom()) {
				memory = nexus->file->loadDZTex(i);
			} else {
				memory = (char *)nexus->file->map(start, size);
			}
			
			out.offset = file.pos()/NEXUS_PADDING;
			if(saveAsDeepzoom) {
				QFile texFile(deepzoomFilePath(deepzoomFolder, i, QStringLiteral("jpg")));
				if(!texFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
					throw QString("could not open texture file %1 for writing").arg(texFile.fileName());
				texFile.write(memory, size);
				texFile.close();
			} else {
				file.write(memory, size);
			}

			if(nexus->header.signature.isDeepzoom()) {
				nexus->file->dropDZTex(memory);
			} else {
				nexus->file->unmap(memory);
			}
		}
		textures.back().offset = file.pos()/NEXUS_PADDING;
	}
	
	file.seek(header_size);
	file.write((char *)&*nodes.begin(), sizeof(nx::Node)*nodes.size());
	file.write((char *)&*patches.begin(), sizeof(nx::Patch)*patches.size());
	file.write((char *)&*textures.begin(), sizeof(nx::Texture)*textures.size());
	file.close();
}


nx::Traversal::Action Extractor::expand(nx::Traversal::HeapNode h) {
	nx::Node &node = nexus->nodes[h.node];
	current_size += node.getEndOffset() - node.getBeginOffset();
	current_triangles += node.nface;
	
	cout << "Max size: " << max_size << " Current size: " << current_size << endl;
	if(max_triangles && current_triangles > max_triangles)
		return STOP;
	if(max_size && current_size > max_size)
		return STOP;
	if(min_error && node.error < min_error)
		return STOP;
	if(max_level >= 0 && nlevels- sinkDistance(h.node) > max_level)
		return STOP;
	
	return EXPAND;
}

float Extractor::nodeError(uint32_t node, bool &visible) {
	if(max_level >= 0) return sinkDistance(node);
	return Traversal::nodeError(node, visible);
}


quint32 Extractor::pad(quint32 s) {
	const quint32 padding = NEXUS_PADDING;
	quint64 m = (s-1) & ~(padding -1);
	return m + padding;
}

void Extractor::compress(QFile &file, nx::Signature &signature, nx::Node &node,
                         nx::NodeData &data, const char *memory, Patch *patches) {
	
	if(signature.flags & Signature::MECO)
		throw "MECO compression is no longer supported";
	if(signature.flags & Signature::CORTO) {
		nx::PlanarVertexData planar(signature.vertex, node.nvert);
		planar.deinterleave(memory);
		
		crt::Encoder encoder(node.nvert, node.nface);
		
		for(uint32_t p = node.first_patch; p < node.last_patch(); p++)
			encoder.addGroup(patches[p].triangle_offset);
		
		auto *coords = reinterpret_cast<const float *>(planar.component(nx::VertexElement::COORD));
		if(node.nface == 0)
			encoder.addPositions(coords, pow(2, coord_q));
		else
			encoder.addPositions(coords, data.faces(signature, node.nvert), pow(2, coord_q));
		
		if(signature.vertex.hasNormals()) {
			auto *normals = reinterpret_cast<const int16_t *>(planar.component(nx::VertexElement::NORM));
			encoder.addNormals(normals, norm_bits,
							   node.nface == 0? crt::NormalAttr::DIFF : crt::NormalAttr::ESTIMATED);
		}
		
		if(signature.vertex.hasColors()) {
			auto *colors = reinterpret_cast<const unsigned char *>(planar.component(nx::VertexElement::COLOR));
			encoder.addColors(colors, color_bits[0], color_bits[1], color_bits[2], color_bits[3]);
		}
		
		if(signature.vertex.hasTextures()) {
			auto *texcoords = reinterpret_cast<const float *>(planar.component(nx::VertexElement::TEX));
			encoder.addUvs(texcoords, tex_step/512);
		}
		encoder.encode();
		
		/*
		int nvert = encoder.nvert;
		int nface = encoder.nface;
		//cout << "Nvert: " << nvert << " Nface: " << nface << " (was: nv: " << node.nvert << " nf: " << node.nface << endl;
		//cout << "Compressed to: " << encoder.stream.size() << endl;
		//cout << "Ratio: " << 100.0f*encoder.stream.size()/(nvert*12 + nface*12) << "%" << endl;
		//cout << "Bpv: " << 8.0f*encoder.stream.size()/nvert << endl << endl;
		
		//cout << "Header: " << encoder.header_size << " bpv: " << (float)encoder.header_size/nvert << endl;
		
		crt::VertexAttribute *coord = encoder.data["position"];
		cout << "Coord bpv; " << 8.0f*coord->size/nvert << " size: " << coord->size << endl;
		cout << "Coord q: " << coord->q << " bits: " << coord->bits << endl << endl;
		
		crt::VertexAttribute *norm = encoder.data["normal"];
		if(norm) {
			cout << "Normal bpv; " << 8.0f*norm->size/nvert << " size: " << norm->size << endl;
			cout << "Normal q: " << norm->q << " bits: " << norm->bits << endl << endl;
		}
		
		crt::ColorAttr *color = dynamic_cast<crt::ColorAttr *>(encoder.data["color"]);
		if(color) {
			cout << "Color bpv; " << 8.0f*color->size/nvert << " size: " << color->size << endl;
			cout << "Color q: " << color->qc[0] << " " << color->qc[1] << " " << color->qc[2] << " " << color->qc[3] << endl;
		}
		
		crt::GenericAttr<int> *uv = dynamic_cast<crt::GenericAttr<int> *>(encoder.data["uv"]);
		if(uv) {
			cout << "Uv bpv; " << 8.0f*uv->size/nvert << endl;
			cout << "Uv q: " << uv->q << " bits: " << uv->bits << endl << endl;
		}
		
		cout << "Face bpv; " << 8.0f*encoder.index.size/nvert << " size: " << encoder.index.size << endl; */
		
		
		
		file.write((char *)&*encoder.stream.data(), encoder.stream.size());
		quint64 size = pad(file.pos()) - file.pos();
		char tmp[NEXUS_PADDING];
		file.write(tmp, size);
	}
}

void Extractor::countElements(quint64 &n_vertices, quint64 &n_faces) {
	uint32_t n_nodes = nexus->header.n_nodes;
	Node *nodes = nexus->nodes;
	Patch *patches = nexus->patches;

	if(!selected.size())
		selected.resize(n_nodes, true);

	selected.back() = false;
	//extracted patches
	n_vertices = 0;
	n_faces = 0;
	std::vector<quint64> offsets(n_nodes, 0);


	for(quint32 i = 0; i < n_nodes-1; i++) {
		if(skipNode(i)) continue;

		Node &node = nodes[i];
		n_vertices += node.nvert;

		uint start = 0;
		for(uint p = node.first_patch; p < node.last_patch(); p++) {
			Patch &patch = patches[p];
			if(!selected[patch.node]) {
				n_faces += patch.triangle_offset - start;
			}
			start = patch.triangle_offset;
		}
	}
}
void Extractor::savePly(QString filename) {
	
	uint32_t n_nodes = nexus->header.n_nodes;
	Node *nodes = nexus->nodes;
	Patch *patches = nexus->patches;
	
	bool has_colors = nexus->header.signature.vertex.hasColors();

	quint64 n_vertices, n_faces;
	countElements(n_vertices, n_faces);
	
	cout << "Vertices: " << n_vertices << endl;
	cout << "Faces: " << n_faces << endl;

	QFile ply(filename);
	if(!ply.open(QFile::ReadWrite)) {
		cerr << "Could not open file: " << qPrintable(filename) << endl;
		exit(-1);
	}

	{ //stram flushes on destruction
		QTextStream stream(&ply);
		stream << "ply\n"
			   << "format binary_little_endian 1.0\n"
				  //<< "format ascii 1.0\n"
			   << "comment generated from nexus\n"
			   << "element vertex " << n_vertices << "\n"
			   << "property float x\n"
			   << "property float y\n"
			   << "property float z\n";
		if(has_colors) {
			stream << "property uchar red\n"
				   << "property uchar green\n"
				   << "property uchar blue\n"
				   << "property uchar alpha\n";
		}
		stream << "element face " << n_faces << "\n"
			   << "property list uchar int vertex_index\n"
			   << "end_header\n";        //qtextstrem adds a \n when closed. stupid.
	}
	
	//writing vertices
	quint32 bytes_per_vertex = 12; //3 floats
	if(has_colors) bytes_per_vertex += 4;
	
	std::vector<quint64> offsets(n_nodes, 0);

	quint64 count_vertices = 0;
	for(uint n = 0; n < n_nodes-1; n++) {
		offsets[n] = count_vertices;
		if(skipNode(n)) continue;
		
		Node &node = nodes[n];
		nexus->loadRam(n);
		NodeData &data = nexus->nodedata[n];
		PlanarVertexData planar(nexus->header.signature.vertex, node.nvert);
		planar.deinterleave(data.memory);
		
		char *buffer = new char[bytes_per_vertex * node.nvert];
		auto *coords = reinterpret_cast<nx::Vector3f *>(planar.component(nx::VertexElement::COORD));
		auto *colors = reinterpret_cast<nx::Rgba8 *>(planar.component(nx::VertexElement::COLOR));
		if(has_colors) {
			struct PlyColorVertex { nx::Vector3f position; nx::Rgba8 color; };
			static_assert(sizeof(PlyColorVertex) == 16, "PLY vertex layout must be packed");
			PlyColorVertex *vertices = reinterpret_cast<PlyColorVertex *>(buffer);
			for(int k = 0; k < node.nvert; k++)
				vertices[k] = {coords[k], colors[k]};
		} else {
			memcpy(buffer, coords, sizeof(nx::Vector3f) * node.nvert);
		}
		count_vertices += n_vertices;
		ply.write((char *)buffer, bytes_per_vertex * node.nvert);
		delete []buffer;
		nexus->dropRam(n);
	}
	
	
	//writing faces
	quint32 bytes_per_face = 1 + 3*sizeof(quint32);
	
	char *buffer = new char[bytes_per_face * (1<<16)];
	
	for(uint n = 0; n < n_nodes-1; n++) {
		
		if(skipNode(n)) continue;
		
		Node &node = nodes[n];
		
		
		uint32_t offset = offsets[n];
		
		nexus->loadRam(n);
		NodeData &data = nexus->nodedata[n];
		uint start = 0;
		for(uint p = node.first_patch; p < node.last_patch(); p++) {
			Patch &patch = patches[p];
			if(selected[patch.node]) {
				start = patch.triangle_offset;
				continue;
			}
			
			uint face_no = patch.triangle_offset - start;
			uint16_t *triangles = data.faces(nexus->header.signature, node.nvert);
			
			char *pos = buffer;
			for(uint k = start; k < patch.triangle_offset; k++) {
				*pos = 3;
				pos++;
				int *f = (int *)pos;
				for(int j = 0; j < 3; j++) {
					*f = offset + (int)triangles[3*k + j];
					f++;
				}
				pos = (char *)f;
			}
			
			ply.write((char *)buffer, bytes_per_face * face_no);
			
			start = patch.triangle_offset;
		}
		nexus->dropRam(n);
	}
	delete []buffer;
	
	
	ply.close();
}


void Extractor::saveStl(QString filename) {

	quint64 n_vertices, n_faces;
	countElements(n_vertices, n_faces);

	uint32_t n_nodes = nexus->header.n_nodes;
	Node *nodes = nexus->nodes;
	Patch *patches = nexus->patches;

	cout << "Vertices: " << n_vertices << endl;
	cout << "Faces: " << n_faces << endl;


	QFile stl(filename);
	if(!stl.open(QFile::ReadWrite)) {
		cerr << "Could not open file: " << qPrintable(filename) << endl;
		exit(-1);
	}
	char header[80] = "STL";
	stl.write(header, 80);

	uint32_t nfaces = uint32_t(n_faces);
	stl.write((char *)&nfaces, 4);

	//each triangles needs 50 bytes (face normal, vertex coords an attribute short (unused)
	char *buffer = new char[50 * (1<<16)];

	for(uint n = 0; n < n_nodes-1; n++) {

		if(skipNode(n)) continue;



		Node &node = nodes[n];
		assert(node.nface <= (1<<16));

		memset(buffer, 0, 50*(1<<16));
		quint64 face_count = 0;

		nexus->loadRam(n);
		NodeData &data = nexus->nodedata[n];
		PlanarVertexData planar(nexus->header.signature.vertex, node.nvert);
		planar.deinterleave(data.memory);
		auto *coords = reinterpret_cast<nx::Vector3f *>(planar.component(nx::VertexElement::COORD));
		uint start = 0;
		for(uint p = node.first_patch; p < node.last_patch(); p++) {
			Patch &patch = patches[p];
			if(selected[patch.node]) {
				start = patch.triangle_offset;
				continue;
			}

			uint16_t *triangles = data.faces(nexus->header.signature, node.nvert);

			for(uint k = start; k < patch.triangle_offset; k++) {
				float *face = reinterpret_cast<float *>(buffer + 50*face_count);
				const nx::Vector3f &p0 = coords[triangles[3*k + 0]];
				const nx::Vector3f &p1 = coords[triangles[3*k + 1]];
				const nx::Vector3f &p2 = coords[triangles[3*k + 2]];
				nx::Vector3f a = p1 - p0;
				nx::Vector3f b = p2 - p0;
				nx::Vector3f normal = {a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x};
				float length = normal.norm();
				if(length > 0) normal = normal / length;
				face[0] = normal.x; face[1] = normal.y; face[2] = normal.z;
				face[3] = p0.x; face[4] = p0.y; face[5] = p0.z;
				face[6] = p1.x; face[7] = p1.y; face[8] = p1.z;
				face[9] = p2.x; face[10] = p2.y; face[11] = p2.z;
				face_count++;
			}

			start = patch.triangle_offset;
		}
		stl.write((char *)buffer, 50 * face_count);
		nexus->dropRam(n);
	}
	delete []buffer;

	stl.close();
}

struct PlyVertex {
	nx::Vector3f p;
	PlyVertex() {}
	PlyVertex(const nx::Vector3f &_p): p(_p){}
};

struct PlyColorVertex {
	nx::Vector3f v;
	nx::Rgba8 c;
	PlyColorVertex(const nx::Vector3f &_v, const nx::Rgba8 &_c): v(_v), c(_c) {}
};

struct PlyFace {
	char t[13];
	PlyFace() { t[0] = 3;}
	int &v(int k) {
		return *(int *)(t + 1 + k*4);
	}
};

void Extractor::saveUnifiedPly(QString filename) {
	
	uint32_t n_nodes = nexus->header.n_nodes;
	Node *nodes = nexus->nodes;
	Patch *patches = nexus->patches;
	
	bool has_colors = nexus->header.signature.vertex.hasColors();
	bool has_faces = nexus->header.signature.face.hasIndex();
	
	if(!selected.size())
		selected.resize(n_nodes, true);
	
	selected.back() = false;
	QFile ply(filename);
	if(!ply.open(QFile::ReadWrite)) {
		cerr << "Could not open file: " << qPrintable(filename) << endl;
		exit(-1);
	}
	//extracted patches
	quint64 n_vertices = 0;
	quint64 n_faces = 0;
	
	vector<PlyColorVertex> color_vertices;
	vector<PlyVertex> vertices;
	vector<PlyFace> faces;
	
	for(quint32 i = 0; i < n_nodes-1; i++) {
		if(skipNode(i)) continue;
		
		Node &node = nodes[i];
		NodeData &data = nexus->nodedata[i];
		nexus->loadRam(i);
		PlanarVertexData planar(nexus->header.signature.vertex, node.nvert);
		planar.deinterleave(data.memory);
		auto *coords = reinterpret_cast<nx::Vector3f *>(planar.component(nx::VertexElement::COORD));
		auto *colors = reinterpret_cast<nx::Rgba8 *>(planar.component(nx::VertexElement::COLOR));
		uint16_t *triangles = data.faces(nexus->header.signature, node.nvert);
		
		
		vector<int> remap(node.nvert, -1);
		uint start = 0;
		
		for(uint p = node.first_patch; p < node.last_patch(); p++) {
			Patch &patch = patches[p];
			
			if(!selected[patch.node]) {
				if(has_faces) {
					for(uint k = start; k < patch.triangle_offset; k++) {
						PlyFace f;
						for(int j = 0; j < 3; j++) {
							int v = (int)triangles[3*k + j];
							if(remap[v] == -1) {
								
								if(has_colors) {
									remap[v] = color_vertices.size();
									color_vertices.push_back(PlyColorVertex(coords[v], colors[v]));
								} else {
									remap[v] = vertices.size();
									vertices.push_back(PlyVertex(coords[v]));
								}
							}
							f.v(j) = remap[v];
						}
						faces.push_back(f);
					}
				} else {
					for(uint k = start; k < patch.triangle_offset; k++) {
						if(has_colors) {
							color_vertices.push_back(PlyColorVertex(coords[k], colors[k]));
						} else {
							vertices.push_back(PlyVertex(coords[k]));
						}
					}
				}
			}
			start = patch.triangle_offset;
		}
		nexus->dropRam(i);
		
	}
	n_vertices = vertices.size()? vertices.size() : color_vertices.size();
	n_faces = faces.size();
	
	
	
	cout << "n vertices: " << n_vertices << endl;
	cout << "n faces: " << n_faces << endl;
	{
		QTextStream stream(&ply);
		stream << "ply\n"
			   << "format binary_little_endian 1.0\n"
				  //<< "format ascii 1.0\n"
			   << "comment generated from nexus\n"
			   << "element vertex " << n_vertices << "\n"
			   << "property float x\n"
			   << "property float y\n"
			   << "property float z\n";
		if(has_colors) {
			stream << "property uchar red\n"
				   << "property uchar green\n"
				   << "property uchar blue\n"
				   << "property uchar alpha\n";
		}
		if(has_faces) {
			stream << "element face " << n_faces << "\n"
				   << "property list uchar int vertex_indices\n";
		}
		stream << "end_header\n";        //qtextstrem adds a \n when closed. stupid.
	}
	
	assert(sizeof(PlyVertex) == 12);
	assert(sizeof(PlyColorVertex) == 16);
	assert(sizeof(PlyFace) == 13);
	
	if(has_colors)
		ply.write((char *)&*color_vertices.begin(),sizeof(PlyColorVertex)*color_vertices.size());
	else
		ply.write((char *)&*vertices.begin(),sizeof(PlyVertex)*vertices.size());
	
	
	ply.write((char *)&*faces.begin(), sizeof(PlyFace)*faces.size());
	
	ply.close();
}

