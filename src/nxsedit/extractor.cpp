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
#include "extractor.h"

#include "../nxszip/meshcoder.h"
//typedef MeshCoder MeshEncoder;
#include <corto/corto.h>

#include <vcg/space/triangle3.h>
using namespace std;
using namespace nx;

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

void Extractor::save(QString output, nx::Signature &signature) {
	QFile file;
	file.setFileName(output);
	if(!file.open(QIODevice::WriteOnly | QFile::Truncate))
		throw QString("could not open file " + output + " for writing");
	
	nx::Header header = nexus->header;
	header.signature = signature;
	header.nvert = 0;
	header.nface = 0;
	
	if(transform)
		header.sphere.Center() = matrix * header.sphere.Center();
	
	vector<nx::Node> nodes;
	vector<nx::Patch> patches;
	vector<nx::Texture> textures;
	
	uint n_nodes = nexus->header.n_nodes;
	vector<int> node_remap(n_nodes, -1);
	
	
	for(uint i = 0; i < n_nodes-1; i++) {
		if(!selected[i]) continue;
		nx::Node node = nexus->nodes[i];
		if(transform)
			node.sphere.Center() = matrix * node.sphere.Center();
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
	
	
	quint64 size = sizeof(nx::Header)  +
			nodes.size()*sizeof(nx::Node) +
			patches.size()*sizeof(nx::Patch) +
			textures.size()*sizeof(nx::Texture);
	size = pad(size);
	
	for(uint i = 0; i < nodes.size(); i++) {
		nodes[i].offset += size/NEXUS_PADDING;
	}
	
	//TODO should actually remove textures not used anymore.
	
	file.write((char *)&header, sizeof(header));
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
		int data_size = nexus->nodes[i].getSize();
		if(transform) {
			memory = new char[node.getSize()];
			memcpy(memory, data.memory, data_size);
			
			for(int k = 0; k < node.nvert; k++) {
				vcg::Point3f &v = ((vcg::Point3f *)memory)[k];
				v = matrix * v;
			}
		}
		if(signature.isCompressed()) {
			compress(file, signature, node, nexus->nodedata[i], &*patches.begin());
		} else {
			file.write(memory, data_size);
		}
		if(transform)
			delete []memory;
		
		nexus->dropRam(i);
	}
	nodes.back().offset = file.pos()/NEXUS_PADDING;
	
	//save textures
	if(textures.size()) {
		for(uint i = 0; i < textures.size()-1; i++) {
			Texture &in = nexus->textures[i];
			Texture &out = textures[i] = in;
			
			quint64 start = in.getBeginOffset();
			quint64 size = in.getSize();
			char *memory = (char *)nexus->file->map(start, size);
			
			out.offset = file.pos()/NEXUS_PADDING;
			file.write(memory, size);
		}
		textures.back().offset = file.pos()/NEXUS_PADDING;
	}
	
	file.seek(sizeof(nx::Header));
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

void Extractor::compress(QFile &file, nx::Signature &signature, nx::Node &node, nx::NodeData &data, Patch *patches) {
	
	//detect first node error which is out of boundary.
	if(signature.flags & Signature::MECO) {
		
		meco::MeshEncoder coder(node, data, patches, signature);
		coder.coord_q = coord_q;
		coder.error = error_factor*node.error;
		coder.norm_q = norm_bits;
		for(int k = 0; k < 4; k++)
			coder.color_q[k] = color_bits[k];
		//HER if signaure has texture was set to (int)log2(tex_step * pow(2, -12)); //here set to -14
		//here we shold set to -log2(max_side/tex_step) where max_side width or height of the associated texture.
		//unfortunately we have no really fast to provide this info.....
		//so assume
		coder.tex_q = -(int)log2(512/tex_step);
		
		coder.encode();
		
		//cout << "V size: " << coder.coord_size << endl;
		//cout << "N size: " << coder.normal_size << endl;
		//cout << "C size: " << coder.color_size << endl;
		//cout << "I size: " << coder.face_size << endl;
		
		file.write((char *)&*coder.stream.buffer, coder.stream.size());
		//padding
		quint64 size = pad(file.pos()) - file.pos();
		char tmp[NEXUS_PADDING];
		file.write(tmp, size);
	} else if(signature.flags & Signature::CORTO) {
		
		crt::Encoder encoder(node.nvert, node.nface);
		
		for(uint32_t p = node.first_patch; p < node.last_patch(); p++)
			encoder.addGroup(patches[p].triangle_offset);
		
		if(node.nface == 0)
			encoder.addPositions((float *)data.coords(), pow(2, coord_q));
		else
			encoder.addPositions((float *)data.coords(), data.faces(signature, node.nvert), pow(2, coord_q));
		
		if(signature.vertex.hasNormals()) {
			encoder.addNormals((int16_t *)data.normals(signature, node.nvert), norm_bits,
							   node.nface == 0? crt::NormalAttr::DIFF : crt::NormalAttr::ESTIMATED);
		}
		
		if(signature.vertex.hasColors())
			encoder.addColors((unsigned char *)data.colors(signature, node.nvert), color_bits[0], color_bits[1], color_bits[2], color_bits[3]);
		
		if(signature.vertex.hasTextures())
			encoder.addUvs((float *)data.texCoords(signature, node.nvert), tex_step/512);
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
		
		char *buffer = new char[bytes_per_vertex * node.nvert];
		char *pos = buffer;
		vcg::Point3f *coords = data.coords();
		vcg::Color4b *colors = data.colors(nexus->header.signature, node.nvert);
		for(int k = 0; k < node.nvert; k++) {
			vcg::Point3f *p = (vcg::Point3f *)pos;
			*p = coords[k];
			p++;
			pos = (char *)p;
			if(has_colors) {
				vcg::Color4b *c  = (vcg::Color4b *)pos;
				*c = colors[k];
				c++;
				pos = (char *)c;
			}
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
		uint start = 0;
		for(uint p = node.first_patch; p < node.last_patch(); p++) {
			Patch &patch = patches[p];
			if(selected[patch.node]) {
				start = patch.triangle_offset;
				continue;
			}

			uint16_t *triangles = data.faces(nexus->header.signature, node.nvert);
			vcg::Point3f *coords = data.coords();

			for(uint k = start; k < patch.triangle_offset; k++) {
				vcg::Point3f *face = (vcg::Point3f *)(buffer + 50*face_count);
				vcg::Point3f &p0 = coords[triangles[3*k + 0]];
				vcg::Point3f &p1 = coords[triangles[3*k + 1]];
				vcg::Point3f &p2 = coords[triangles[3*k + 2]];

				face[0] = (( p1 - p0) ^ (p2 - p0)).Normalize();
				face[1] = p0;
				face[2] = p1;
				face[3] = p2;
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
	vcg::Point3f p;
	PlyVertex() {}
	PlyVertex(const vcg::Point3f &_p): p(_p){}
};

struct PlyColorVertex {
	vcg::Point3f v;
	vcg::Color4b c;
	PlyColorVertex(const vcg::Point3f &_v, const vcg::Color4b &_c): v(_v), c(_c) {}
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
		
		vcg::Point3f *coords = data.coords();
		vcg::Color4b *colors = data.colors(nexus->header.signature, node.nvert);
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

