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
#include <QDebug>
#include <QThreadPool>
#include <QRunnable>
#include <QFileInfo>
#include <QPainter>
#include <QImage>
#include <QDir>
#include <QImageWriter>
#include "vertex_cache_optimizer.h"

#include "nexusbuilder.h"
#include "kdtree.h"
#include "meshstream.h"
#include "mesh.h"
#include "tmesh.h"
#include "../nxsbuild/gltfbuilder.h"
#include "../nxsbuild/tileset.h"

#include <vcg/math/similarity2.h>
#include <vcg/space/rect_packer.h>

#include <iostream>
using namespace std;

using namespace nx;

static qint64 pad(qint64 s) {
	const qint64 padding = NEXUS_PADDING;
	qint64 m = (s-1) & ~(padding -1);
	return m + padding;
}



unsigned int nextPowerOf2 ( unsigned int n )
{
	unsigned count = 0;

	// First n in the below condition
	// is for the case where n is 0
	if (n && !(n & (n - 1)))
		return n;

	while( n != 0)
	{
		n >>= 1;
		count += 1;
	}

	return 1 << count;
}


NodeBox::NodeBox(KDTree *tree, uint32_t block) {
	for(int k = 0; k < 3; k++)
		axes[k] = tree->axes[k];
	box = tree->block_boxes[block];
}

bool NodeBox::isIn(vcg::Point3f &p) {
	return KDTree::isIn(axes, box, p);
}

vector<bool> NodeBox::markBorders(Node &node, vcg::Point3f *p, uint16_t *f) {
	vector<bool> border(node.nvert, false);
	for(int i = 0; i < node.nface; i++) {
		bool outside = false;
		for(int k = 0; k < 3; k++) {
			uint16_t index = f[i*3 + k];
			outside |= !isIn(p[index]);
		}
		if(outside)
			for(int k = 0; k < 3; k++) {
				uint16_t index = f[i*3 + k];
				border[index] = true;
			}
	}
	return border;
}

NexusBuilder::NexusBuilder(quint32 components): chunks("cache_chunks"), scaling(0.5), useNodeTex(true), tex_quality(92), nodeTex("cache_tex") {

	Signature &signature = header.signature;
	signature.vertex.setComponent(VertexElement::COORD, Attribute(Attribute::FLOAT, 3));
	if(components & FACES)     //ignore normals for meshes
		signature.face.setComponent(FaceElement::INDEX, Attribute(Attribute::UNSIGNED_SHORT, 3));
	if(components & NORMALS)
		signature.vertex.setComponent(VertexElement::NORM, Attribute(Attribute::SHORT, 3));
	if(components & COLORS)
		signature.vertex.setComponent(VertexElement::COLOR, Attribute(Attribute::BYTE, 4));
	if(components & TEXTURES)
		signature.vertex.setComponent(FaceElement::TEX, Attribute(Attribute::FLOAT, 2));

	header.version = 2;
	header.signature = signature;
	header.nvert = header.nface = header.n_nodes = header.n_patches = header.n_textures = 0;

	nodeTex.open();
}

NexusBuilder::NexusBuilder(Signature &signature): chunks("cache_chunks"), scaling(0.5) {
	header.version = 2;
	header.signature = signature;
	header.nvert = header.nface = header.n_nodes = header.n_patches = header.n_textures = 0;
}

void NexusBuilder::initAtlas(std::vector<QImage>& textures) {
	if(textures.size()) {
		atlas.addTextures(textures);
	}
}

bool NexusBuilder::initAtlas(std::vector<LoadTexture> &textures) {
	if(textures.size()) {
		bool success = atlas.addTextures(textures);
		if(!success)
			return false;
	}
	return true;
}

void NexusBuilder::create(KDTree *tree, Stream *stream, uint top_node_size) {
	Node sink;
	sink.first_patch = 0;
	nodes.push_back(sink);

	int level = 0;
	int last_top_level_size = 0;
	do {
		tree->clear();
		if(level % 2) tree->setAxesDiagonal();
		else tree->setAxesOrthogonal();

		tree->load(stream);
		stream->clear();

		createLevel(tree, stream, level);
		level++;
		if(skipSimplifyLevels <= 0 && last_top_level_size != 0 && stream->size()/(float)last_top_level_size > 0.9f) {
			cout << "Stream: " << stream->size() << " Last top level size: " << last_top_level_size << endl;
			cout << "Larger top level, most probably to high parametrization fragmentation.\n";
			break;
		}
		last_top_level_size = stream->size();
		skipSimplifyLevels--;
	} while(stream->size() > top_node_size);

	reverseDag();
	saturate();
}

class UnionFind {
public:
	std::vector<int> parents;
	void init(int size) {
		parents.resize(size);
		for(int i = 0; i < size; i++)
			parents[i] = i;
	}

	int root(int p) {
		while(p != parents[p])
			p = parents[p] = parents[parents[p]];
		return p;
	}
	void link(int p0, int p1) {
		int r0 = root(p0);
		int r1 = root(p1);
		parents[r1] = r0;
	}
	int compact(std::vector<int> &node_component) { //change numbering of the connected components, return number
		node_component.resize(parents.size());
		std::map<int, int> remap;// inser root here and order them.
		for(size_t i = 0; i < parents.size(); i++) {
			int root = i;
			while(root != parents[root])
				root = parents[root];
			parents[i] = root;
			node_component[i] = remap.emplace(root, remap.size()).first->second;
		}
		return remap.size();
	}

};

QImage NexusBuilder::extractNodeTex(TMesh &mesh, int level, float &error, float &pixelXedge) {
	std::vector<vcg::Box2f> boxes;
	std::vector<int> box_texture; //which texture each box belongs;
	std::vector<int> vertex_to_tex(mesh.vert.size(), -1);
	std::vector<int> vertex_to_box;


	//find connected pieces of triangles belonging to the same texture
	//iterate over the triangles and connect the vertices.
	UnionFind components;
	components.init(mesh.vert.size());

	for(auto &face: mesh.face) {
		int v[3];
		for(int i = 0; i < 3; i++) {
			v[i] = face.V(i) - &*mesh.vert.begin();


			int &t = vertex_to_tex[v[i]];

			if(t != -1 && t != face.tex) qDebug() << "Missing vertex replication across seams\n";
			t = face.tex;
		}
		components.link(v[0], v[1]);
		components.link(v[0], v[2]);
		assert(components.root(v[1]) == components.root(v[2]));
	}
	int n_boxes = components.compact(vertex_to_box);

	//assign a texture to each vertex (we already split)
	for(auto &face: mesh.face) {
		int v[3];
		for(int i = 0; i < 3; i++) {
			int v = face.V(i) - &*mesh.vert.begin();
			vertex_to_tex[v] = face.tex;
		}
	}

	//assign all boxes to a tex (and remove boxes where the tex is -1

	//compute boxes
	boxes.resize(n_boxes);
	box_texture.resize(n_boxes, -1);
	for(size_t i = 0; i < mesh.vert.size(); i++) {
		int b = vertex_to_box[i];
		int tex = vertex_to_tex[i];
		if(tex < 0) continue; //vertex not assigned.

		vcg::Box2f &box = boxes[b];
		box_texture[b] = tex;
		auto &t = mesh.vert[i].T().P();
		if(t[0] != 1.0)
			t[0] = fmod(t[0], 1.0);
		if(t[1] != 1.0)
			t[1] = fmod(t[1], 1.0);

		if(t[0] != 0.0f || t[1] != 0.0f)
			box.Add(t);
	}

	//erase boxes assigned to no texture, and remap vertex_to_box
	int count = 0;
	std::vector<int> remap(mesh.vert.size(), -1);
	for(int i = 0; i < n_boxes; i++) {
		if(box_texture[i] == -1)
			continue;
		boxes[count] = boxes[i];
		box_texture[count] = box_texture[i];
		remap[i] = count++;
	}
	boxes.resize(count);
	box_texture.resize(count);
	for(int &b: vertex_to_box)
		b = remap[b];

	std::vector<vcg::Point2i> sizes(boxes.size());
	std::vector<vcg::Point2i> origins(boxes.size());
	for(size_t b = 0; b < boxes.size(); b++) {
		auto &box = boxes[b];
		if(box.DimX() > 0.9) {
			for(auto &face: mesh.face) {
				int v[3];
				for(int i = 0; i < 3; i++) {
					auto v = face.V(i);
					int j = (i+1)%3;
				}
			}
		}
		int tex = box_texture[b];

		float w = atlas.width(tex, level); //img->size().width();
		float h = atlas.height(tex, level); //img->size().height();
		float px = 1/(float)w;
		float py = 1/(float)h;
		box.Offset(vcg::Point2f(px, py));
		//snap to higher pix (clamped by 0 and 1 anyway)
		vcg::Point2i &size = sizes[b];
		vcg::Point2i &origin = origins[b];
		origin[0] = std::max(0.0f, floor(box.min[0]/px));
		origin[1] = std::max(0.0f, floor(box.min[1]/py));
		if(origin[0] >= w) origin[0] = w-1;
		if(origin[1] >= h) origin[1] = h-1;

		assert(origin[0] >= 0);

		size[0] = std::min(w, ceil(box.max[0]/px)) - origin[0];
		size[1] = std::min(h, ceil(box.max[1]/py)) - origin[1];
		if(size[0] <= 0) size[0] = 1;
		if(size[1] <= 0) size[1] = 1;
	}

	//pack boxes;
	std::vector<vcg::Point2i> mapping;
	vcg::Point2i maxSize(1096, 1096);
	vcg::Point2i finalSize;
	bool success = false;
	for(int i = 0; i < 5; i++, maxSize[0]*= 2, maxSize[1]*= 2) {
		if(sizes.size() == 0) { //no texture!
			finalSize = vcg::Point2i(1, 1);
			success = true;
			break;
		}
		bool too_large = false;
		for(auto s: sizes) {
			if(s[0] >= maxSize[0] || s[1] >= maxSize[1])
				too_large = true;
		}
		if(too_large) { //TODO the packer should simply return false
			continue;
		}
		mapping.clear(); //TODO this should be done inside the packer
		success = vcg::RectPacker<float>::PackInt(sizes, maxSize, mapping, finalSize);
		if(success)
			break;
	}
	if(!success) {
		cerr << "Failed packing: the texture in a single nexus node would be > 16K\n";
		cerr << "Try to reduce the size of the nodes using -t (default is 4096)";
		exit(0);
	}

	if (createPowTwoTex) {
		finalSize[ 0 ] = (int) nextPowerOf2( finalSize[ 0 ] );
		finalSize[ 1 ] = (int) nextPowerOf2( finalSize[ 1 ] );
	}

	QImage image(finalSize[0], finalSize[1], QImage::Format_RGB32);
	image.fill(QColor(127, 127, 127));
	//copy boxes using mapping

	float pdx = 1/(float)image.width();
	float pdy = 1/(float)image.height();

	for(size_t i = 0; i < mesh.vert.size(); i++) {
		auto &p = mesh.vert[i];
		auto &uv = p.T().P();
		int b = vertex_to_box[i];
		if(b == -1) {
			uv = vcg::Point2f(0.0f, 0.0f);
			continue;
		}
		vcg::Point2i &o = origins[b];
		vcg::Point2i m = mapping[b];

		int tex = box_texture[b];
		float w = atlas.width(tex, level);
		float h = atlas.height(tex, level);
		float px = 1/(float)w;
		float py = 1/(float)h;

		if(uv[0] < 0.0f)
			uv[0] = 0.0f;
		if(uv[1] < 0.0f)
			uv[1] = 0.0f;

		float dx = uv[0]/px - o[0];
		float dy = uv[1]/py - o[1];
		if(dx < 0.0f) dx = 0.0f;
		if(dy < 0.0f) dy = 0.0f;

		uv[0] = (m[0] + dx)*pdx; //how many pixels from the origin
		uv[1] = (m[1] + dy)*pdy; //how many pixels from the origin

		assert(!isnan(uv[0]));
		assert(!isnan(uv[1]));
	}
	//compute error:
	float pdx2 = pdx*pdx;
	error = 0.0;
	pixelXedge = 0.0f;
	for(auto &face: mesh.face) {
		for(int k = 0; k < 3; k++) {
			int j = (k==2)?0:k+1;

			float edge = vcg::SquaredNorm(face.P(k) - face.P(j));
			float pixel = vcg::SquaredNorm(face.V(k)->T().P() - face.V(j)->T().P())/pdx2;
			pixelXedge += pixel;
			if(pixel > 10) pixel = 10;
			if(pixel < 1)
				error += edge;
			else
				error += edge/pixel;
		}
	}
	pixelXedge = sqrt(pixelXedge/mesh.face.size()*3);
	error = sqrt(error/mesh.face.size()*3);
	
	double areausage = 0.0;
	//compute area waste
	for(int i = 0; i < mesh.face.size(); i++) {
		auto &face = mesh.face[i];
		int b = vertex_to_box[face.V(0) - &(mesh.vert[0])];
		vcg::Point2i &o = origins[b];
		vcg::Point2i m = mapping[b];
		auto V0 = face.V(0)->T().P();
		auto V1 = face.V(1)->T().P();
		auto V2 = face.V(2)->T().P();
		areausage += (V2 - V0)^(V2 - V1)/2;
	}

	{
		QMutexLocker locker(&m_atlas);
		//	static int boxid = 0;
		QPainter painter(&image);
		//convert tex coordinates using mapping
		for(int i = 0; i < boxes.size(); i++) {

			/*		vcg::Color4b  color;
		color[2] = ((boxid % 11)*171)%63 + 63;
		//color[1] = 255*log2((i+1))/log2(nexus->header.n_patches);
		color[1] = ((boxid % 7)*57)%127 + 127;
		color[0] = ((boxid % 16)*135)%127 + 127; */

			int source = box_texture[i];
			vcg::Point2i &o = origins[i];
			vcg::Point2i &s = sizes[i];

			QImage rect = atlas.read(source, level, QRect(o[0], o[1], s[0], s[1]));
			painter.drawImage(mapping[i][0], mapping[i][1], rect);

			//		painter.fillRect(mapping[i][0], mapping[i][1], s[0], s[1], QColor(color[0], color[1], color[2]));
			//		boxid++;
		}

		
		/*	painter.setPen(QColor(255,0,255));
		for(int i = 0; i < mesh.face.size(); i++) {
			auto &face = mesh.face[i];
			int b = vertex_to_box[face.V(0) - &(mesh.vert[0])];
			vcg::Point2i &o = origins[b];
			vcg::Point2i m = mapping[b];
			
			for(int k = 0; k < 3; k++) {
				int j = (k==2)?0:k+1;
				auto V0 = face.V(k);
				auto V1 = face.V(j);
				float x0 = V0->T().P()[0]/pdx; //how many pixels from the origin
				float y0 = V0->T().P()[1]/pdy; //how many pixels from the origin
				float x1 = V1->T().P()[0]/pdx; //how many pixels from the origin
				float y1 = V1->T().P()[1]/pdy; //how many pixels from the origin
				painter.drawLine(x0, y0, x1, y1);
			}
		}*/
		/*
		for(int i = 0; i < mesh.vert.size(); i++) {
			auto &p = mesh.vert[i];
			int b = vertex_to_box[i];
			vcg::Point2i &o = origins[b];
			vcg::Point2i m = mapping[b];


			float x = p.T().P()[0]/pdx; //how many pixels from the origin
			float y = p.T().P()[1]/pdy; //how many pixels from the origin
			painter.setPen(QColor(255,0,255));
			painter.drawEllipse(x-10, y-10, 20, 20);
			painter.drawPoint(x, y);
		} */

	}

	image = image.mirrored();
	//static int imgcount = 0;
	//image.save(QString("OUT_test_%1.jpg").arg(imgcount++));
	return image;
}

void NexusBuilder::createCloudLevel(KDTreeCloud *input, StreamCloud *output, int level) {

	for(uint block = 0; block < input->nBlocks(); block++) {
		Cloud cloud = input->get(block);
		assert(cloud.size() < (1<<16));
		if(cloud.size() == 0) continue;

		Mesh mesh;
		mesh.load(cloud);

		int target_points = cloud.size()*scaling;
		std::vector<AVertex> deleted = mesh.simplifyCloud(target_points);

		//save node in nexus temporary structure
		quint32 mesh_size = mesh.serializedSize(header.signature);
		mesh_size = pad(mesh_size);
		quint32 chunk = chunks.addChunk(mesh_size);
		uchar *buffer = chunks.getChunk(chunk);
		quint32 patch_offset = patches.size();

		std::vector<Patch> node_patches;
		mesh.serialize(buffer, header.signature, node_patches);

		//patches will be reverted later, but the local order is important because of triangle_offset
		std::reverse(node_patches.begin(), node_patches.end());
		patches.insert(patches.end(), node_patches.begin(), node_patches.end());

		quint32 current_node = nodes.size();
		nx::Node node = mesh.getNode();
		node.offset = chunk; //temporaryle remember which chunk belongs to which node
		node.error = mesh.averageDistance();
		node.first_patch = patch_offset;
		nodes.push_back(node);
		boxes.push_back(NodeBox(input, block));

		//we pick the deleted vertices from simplification and reprocess them.

		swap(mesh.vert, deleted);
		mesh.vn = mesh.vert.size();

		Splat *vertices = new Splat[mesh.vn];
		mesh.getVertices(vertices, current_node);

		for(int i = 0; i < mesh.vn; i++) {
			Splat &s = vertices[i];
			output->pushVertex(s);
		}

		delete []vertices;
	}
}



/*
  Commented because the gain is negligible ( and the code is not correct either, there is
  some problem in saving the trianglws... */


class Worker: public QRunnable {
public:
	int level;
	uint block;
	KDTreeSoup *input;
	StreamSoup *output;
	NexusBuilder &builder;

	Worker(NexusBuilder &_builder, KDTreeSoup *in, StreamSoup *out, uint _block, int _level):
		builder(_builder), input(in), output(out), block(_block), level(_level) {}

protected:
	void run() {
		builder.processBlock(input, output, block, level);
	}
};


void NexusBuilder::processBlock(KDTreeSoup *input, StreamSoup *output, uint block, int level) {
	TMesh mesh;
	TMesh tmp; //this is needed saving a mesh with vertices on seams duplicated., and for node tex coordinates to be rearranged

	Mesh mesh1;
	quint32 mesh_size;


	int ntriangles = 0;
	{
		QMutexLocker locker(&m_input);
		Soup soup = input->get(block); //soup is memory allocated by input, lock is needed.
		assert(soup.size() < (1<<16));
		if(soup.size() == 0) return;

		ntriangles = soup.size();
		if(!hasTextures()) {
			mesh1.load(soup);
		} else {
			mesh.load(soup);
		}
	}



	if(!hasTextures()) {
		//no need to mutex the input, it won't change anything.
		input->lock(mesh1, block);
		mesh_size = mesh1.serializedSize(header.signature);

	} else {

		input->lock(mesh, block);
		//we need to replicate vertices where textured seams occours

		vcg::tri::Append<TMesh,TMesh>::MeshCopy(tmp,mesh);
		for(int i = 0; i < tmp.face.size(); i++) {
			tmp.face[i].node = mesh.face[i].node;
			tmp.face[i].tex = mesh.face[i].tex;
		}
		tmp.splitSeams(header.signature);
		if(tmp.vert.size() > 60000) {
			cerr << "Unable to properly simplify due to fragmented parametrization\n"
				 << "Try to reduce the size of the nodes using -f (default is 32768)" << endl;
			exit(0);
		}

		//save node in nexus temporary structure
		mesh_size = tmp.serializedSize(header.signature);
	}
	mesh_size = pad(mesh_size);
	uchar *buffer = new uchar[mesh_size];

	std::vector<Patch> node_patches;

	float error;
	float pixelXedge;
	if(!hasTextures()) {
		mesh1.serialize(buffer, header.signature, node_patches);


	} else {

		if(useNodeTex) {
			QImage nodetex = extractNodeTex(tmp, level, error, pixelXedge);
			tmp.serialize(buffer, header.signature, node_patches);

			Texture t;

			{
				QMutexLocker locker(&m_textures);
				t.offset = nodeTex.size()/NEXUS_PADDING;

				output_pixels += nodetex.width()*nodetex.height();

				QImageWriter writer(&nodeTex, "jpg");
				writer.setQuality(tex_quality);
#if QT_VERSION >= QT_VERSION_CHECK(5, 5, 0)
				writer.setOptimizedWrite(true);
				writer.setProgressiveScanWrite(true);
#endif
				writer.write(nodetex);

				quint64 size = pad(nodeTex.size());
				nodeTex.resize(size);
				nodeTex.seek(size);
			}
			{
				QMutexLocker locker(&m_builder);
				textures.push_back(t);
				for(Patch &patch: node_patches)
					patch.texture = textures.size()-1; //last texture inserted
			}

			//#define DEBUG_TEXTURES
#ifdef DEBUG_TEXTURES
			static int counter = 0;

			QString texname = QString::number(counter) + ".jpg";
			QImageWriter rewriter(texname, "jpg");
			rewriter.setQuality(tex_quality);
			rewriter.write(nodetex);

			tmp.textures.push_back(texname.toStdString());
			tmp.savePlyTex(QString::number(counter) + ".ply", texname);
			counter++;
#endif
		}

		//VICIUOS TRICK: we could save only a texture every 2 geometry levels since the patch is contained also to a parent node.
		//we could store the texture in the parent nodes i and have it good also for the children node.
		//but only starting from the bottom.

	}
	quint32 chunk;
	//done serializing, move the data to the chunk.
	{
		QMutexLocker locker(&m_chunks);
		chunk = chunks.addChunk(mesh_size);
		uchar *chunk_buffer = chunks.getChunk(chunk);
		memcpy(chunk_buffer, buffer, mesh_size);
		chunks.dropChunk(chunk); //no neede anymore
	}
	delete []buffer;



	nx::Node node;
	if(!hasTextures())
		node = mesh1.getNode(); //get node data before simplification
	else
		node = tmp.getNode();


	int nface;
	{

		if(!hasTextures()) {
			mesh1.lockVertices();
			{ //needed only if Mesh::QUADRICS
				QMutexLocker locker(&m_texsimply);
				mesh1.quadricInit();
			}
			error = mesh1.simplify(ntriangles*scaling, Mesh::QUADRICS);
			nface = mesh1.fn;

		} else {
			QMutexLocker locker(&m_texsimply);
			int nvert = ntriangles*scaling;

			if(skipSimplifyLevels > 0)
				nvert = ntriangles;

			else if(nvert < 64) //don't simplify too much!
				nvert = 64;

			float e = mesh.simplify(nvert, TMesh::QUADRICS);
			if(!useNodeTex)
				error = e;
			nface = mesh.fn;
		}
	}


	quint32 current_node;
	{
		QMutexLocker locker(&m_builder);

		//patches will be reverted later, but the local order is important because of triangle_offset
		quint32 patch_offset = patches.size();
		std::reverse(node_patches.begin(), node_patches.end());
		patches.insert(patches.end(), node_patches.begin(), node_patches.end());

		current_node = nodes.size();
		node.offset = chunk;          //temporarily remember which chunk belongs to which node
		node.error = error;


		node.first_patch = patch_offset;

		nodes.push_back(node);
		boxes.push_back(NodeBox(input, block));
	}


	//Simplify and stream the meshes





	Triangle *triangles = new Triangle[nface];
	//streaming the output
	if(!hasTextures()) {
		mesh1.getTriangles(triangles, current_node);
	} else {
		mesh.getTriangles(triangles, current_node);
	}

	{
		QMutexLocker locker(&m_output);
		for(int i = 0; i < nface; i++) {
			Triangle &t = triangles[i];
			if(!t.isDegenerate())
				output->pushTriangle(triangles[i]);
		}
	}
	delete []triangles;
}

void NexusBuilder::createMeshLevel(KDTreeSoup *input, StreamSoup *output, int level) {
	atlas.buildLevel(level);
	if(level > 0)
		atlas.flush(level-1);


	QThreadPool pool;
	pool.setMaxThreadCount(n_threads);

	for(uint block = 0; block < input->nBlocks(); block++) {
		Worker *worker = new Worker(*this, input, output, block, level);
		pool.start(worker);
	}
	pool.waitForDone();
}


void NexusBuilder::createLevel(KDTree *in, Stream *out, int level) {
	KDTreeSoup *isSoup = dynamic_cast<KDTreeSoup *>(in);
	if(!isSoup) {
		KDTreeCloud *input = dynamic_cast<KDTreeCloud *>(in);
		StreamCloud *output = dynamic_cast<StreamCloud *>(out);
		createCloudLevel(input, output, level);
	} else {
		KDTreeSoup *input = dynamic_cast<KDTreeSoup *>(in);
		StreamSoup *output = dynamic_cast<StreamSoup *>(out);
		createMeshLevel(input, output, level);
	}
}

void NexusBuilder::saturate() {
	//we do not have the 'backlinks' so we make a depth first traversal
	//TODO! BIG assumption: the nodes are ordered such that child comes always after parent
	for(int node = nodes.size()-2; node >= 0; node--)
		saturateNode(node);

	//nodes.front().error = nodes.front().sphere.Radius()/2;
	nodes.back().error = 0;
}

void NexusBuilder::testSaturation() {

	//test saturation:
	for(uint n = 0; n < nodes.size()-1; n++) {
		Node &node = nodes[n];
		vcg::Sphere3f &sphere = node.sphere;
		for(uint p = node.first_patch; p < node.last_patch(); p++) {
			Patch &patch = patches[p];
			Node &child = nodes[patch.node];
			vcg::Sphere3f s = child.sphere;
			float dist = (sphere.Center() - s.Center()).Norm();
			float R = sphere.Radius();
			float r = s.Radius();
			assert(sphere.IsIn(child.sphere));
			assert(child.error < node.error);
		}
	}
}

void NexusBuilder::reverseDag() {

	std::reverse(nodes.begin(), nodes.end());
	std::reverse(boxes.begin(), boxes.end());
	std::reverse(patches.begin(), patches.end());

	//first reversal: but we point now to the last_patch, not the first
	for(uint i = 0; i < nodes.size(); i++)
		nodes[i].first_patch = patches.size() -1 - nodes[i].first_patch;

	//get the previous node last +1 to become the first
	for(uint i = nodes.size()-1; i >= 1; i--)
		nodes[i].first_patch = nodes[i-1].first_patch +1;
	nodes[0].first_patch  = 0;

	//and reversed the order of the nodes.
	for(uint i = 0; i < patches.size(); i++) {
		patches[i].node = nodes.size() - 1 - patches[i].node;
	}
}


void NexusBuilder::save(QString filename) {

	//cout << "Input squaresize " << sqrt(input_pixels) <<  " Output size " << sqrt(output_pixels) << "\n";
	file.setFileName(filename);
	if(!file.open(QIODevice::ReadWrite | QIODevice::Truncate))
		throw QString("could not open file " + filename);

	if(header.signature.vertex.hasNormals() && header.signature.face.hasIndex())
		uniformNormals();

	if(textures.size())
		textures.push_back(Texture());

	header.nface = 0;
	header.nvert = 0;
	header.n_nodes = nodes.size();
	header.n_patches = patches.size();

	header.n_textures = textures.size();
	header.version = 2;

	//find roots and adjust error
	uint32_t nroots = header.n_nodes;
	for(uint32_t j = 0; j < nroots; j++) {
		for(uint32_t i = nodes[j].first_patch; i < nodes[j].last_patch(); i++)
			if(patches[i].node < nroots)
				nroots = patches[i].node;
		nodes[j].error = nodes[j].tight_radius;
	}

	header.sphere = vcg::Sphere3f();
	for(uint32_t i = 0; i < nroots; i++)
		header.sphere.Add(nodes[i].tightSphere());

	for(uint i = 0; i < nodes.size()-1; i++) {
		nx::Node &node = nodes[i];
		header.nface += node.nface;
		header.nvert += node.nvert;
	}

	quint64 size = sizeof(Header)  +
			nodes.size()*sizeof(Node) +
			patches.size()*sizeof(Patch) +
			textures.size()*sizeof(Texture);
	size = pad(size);
	quint64 index_size = size;

	std::vector<quint32> node_chunk; //for each node the corresponding chunk
	for(quint32 i = 0; i < nodes.size()-1; i++)
		node_chunk.push_back(nodes[i].offset);

	//compute offsets and store them in nodes
	for(uint i = 0; i < nodes.size()-1; i++) {
		nodes[i].offset = size/NEXUS_PADDING;
		quint32 chunk = node_chunk[i];
		size += chunks.chunkSize(chunk);
	}
	nodes.back().offset = size/NEXUS_PADDING;

	if(textures.size()) {
		if(!useNodeTex) {
			for(uint i = 0; i < textures.size()-1; i++) {
				quint32 s = textures[i].offset;
				textures[i].offset = size/NEXUS_PADDING;
				size += s;
				size = pad(size);
			}
			textures.back().offset = size/NEXUS_PADDING;

		} else { //texture.offset keeps the index in the nodeTex temporay file (already padded and in NEXUS_PADDING units
			if(header.signature.flags & Signature::Flags::DEEPZOOM) {
				//just fix the last one
				textures.back().offset = nodeTex.size()/NEXUS_PADDING;

			} else { //texture.offset holds the size of each texture
				for(uint i = 0; i < textures.size()-1; i++)
					textures[i].offset += size/NEXUS_PADDING;
				size += nodeTex.size();
				textures.back().offset = size/NEXUS_PADDING;
			}
		}
	}

	qint64 r = file.write((char*)&header, sizeof(Header));
	if(r == -1)
		throw(file.errorString());
	assert(nodes.size());
	file.write((char*)&(nodes[0]), sizeof(Node)*nodes.size());
	if(patches.size())
		file.write((char*)&(patches[0]), sizeof(Patch)*patches.size());
	if(textures.size())
		file.write((char*)&(textures[0]), sizeof(Texture)*textures.size());
	file.seek(index_size);

	//NODES
	QString basename = filename.left(filename.size() - 4) + "_files";
	if(header.signature.flags & Signature::Flags::DEEPZOOM) {
		QDir dir;
		dir.mkdir(basename);
	}
	for(uint i = 0; i < node_chunk.size(); i++) {
		quint32 chunk = node_chunk[i];
		uchar *buffer = chunks.getChunk(chunk);
		optimizeNode(i, buffer);
		if(header.signature.flags & Signature::Flags::DEEPZOOM) {
			QFile nodefile(QString("%1/%2.nxn").arg(basename).arg(i));
			nodefile.open(QFile::WriteOnly);
			nodefile.write((char*)buffer, chunks.chunkSize(chunk));
		} else
			file.write((char*)buffer, chunks.chunkSize(chunk));
	}

	//TEXTURES
	//	QString basename = filename.left(filename.length()-4);
	//compute textures offsetse
	//Image should store all the mipmaps
	if(textures.size()) {
		if(useNodeTex) {
			//todo split into pieces.
			if(header.signature.flags & Signature::Flags::DEEPZOOM) {
				for(uint i = 0; i < textures.size()-1; i++) {
					quint32 s = textures[i].offset*NEXUS_PADDING;
					quint32 size = textures[i+1].offset*NEXUS_PADDING -s;

					nodeTex.seek(s);
					auto buffer = nodeTex.read(size);

					QFile texfile(QString("%1/%2.jpg").arg(basename).arg(i));
					texfile.open(QFile::WriteOnly);
					texfile.write(buffer);
				}
			} else {
				nodeTex.seek(0);

				qint64 buffer_size = 64*1<<20; //64 MB
				do {
					auto buffer = nodeTex.read(buffer_size);
					if(!buffer.size())
						break;

					bool success = file.write(buffer);
					if(!success)
						throw QString("failed writing texture data from temporary file.");
				} while(1);
			}
		} else {
			for(int i = 0; i < textures.size()-1; i++) {
				Texture &tex = textures[i];
				assert(tex.offset == file.pos()/NEXUS_PADDING);
				QFile image(images[i]);
				if(!image.open(QFile::ReadOnly))
					throw QString("could not load img %1").arg(images[i]);
				bool success = file.write(image.readAll());
				if(!success)
					throw QString("failed writing texture %1").arg(images[i]);
				//we should use texture.offset instead.
				quint64 s = file.pos();
				s = pad(s);
				file.resize(s);
				file.seek(s);
			}
		}
	}
	if(textures.size())
		for(int i = 0; i < textures.size()-1; i++) {
			QFile::remove(QString("nexus_tmp_tex%1.png").arg(i));
		}
	cout << "Saving to file " << qPrintable(filename) << endl;
	file.close();
}

void NexusBuilder::exportAsTileset() {

    if(header.signature.vertex.hasNormals() && header.signature.face.hasIndex())
        uniformNormals();

    if(textures.size())
        textures.push_back(Texture());

    quint64 size = sizeof(Header)  +
                   nodes.size()*sizeof(Node) +
                   patches.size()*sizeof(Patch) +
                   textures.size()*sizeof(Texture);
    size = pad(size);

    if(textures.size()) {
        if(!useNodeTex) {
            for(uint i = 0; i < textures.size()-1; i++) {
                quint32 s = textures[i].offset;
                textures[i].offset = size/NEXUS_PADDING;
                size += s;
                size = pad(size);
            }
            textures.back().offset = size/NEXUS_PADDING;

        } else {
            textures.back().offset = nodeTex.size()/NEXUS_PADDING;
        }
    }

    GltfBuilder builder(*this);
    builder.generateTiles();
    TilesetJSON tileset(*this);
    tileset.generate();
}

//include sphere of the children and ensure error s bigger.
void NexusBuilder::saturateNode(quint32 n) {
	const float epsilon = 1.01f;

	nx::Node &node = nodes[n];
	for(quint32 i = node.first_patch; i < node.last_patch(); i++) {
		nx::Patch &patch = patches[i];
		if(patch.node == nodes.size()-1) //sink, get out
			return;

		nx::Node &child = nodes[patch.node];
		if(node.error <= child.error)
			node.error = child.error*epsilon;

		//we cannot just add the sphere, because it moves the center and the tight radius will be wrong
		if(!node.sphere.IsIn(child.sphere)) {
			float dist = (child.sphere.Center() - node.sphere.Center()).Norm();
			dist += child.sphere.Radius();
			if(dist > node.sphere.Radius())
				node.sphere.Radius() = dist;
		}
	}
	node.sphere.Radius() *= epsilon;
}

void NexusBuilder::optimizeNode(quint32 n, uchar *chunk) {
	return;
	Node &node = nodes[n];
	assert(node.nface);

	uint16_t *faces = (uint16_t  *)(chunk + node.nvert*header.signature.vertex.size());

	uint start =  0;
	for(uint i = node.first_patch; i < node.last_patch(); i++) {
		Patch &patch = patches[i];
		uint end = patch.triangle_offset;
		uint nface = end - start;
		//optimizing vertex cache.
		quint16 *triangles = new quint16[nface*3];

		bool success = vmath::vertex_cache_optimizer::optimize_post_tnl(24, faces + 3*start, nface, node.nvert,  triangles);
		if(success)
			memcpy(faces + start, triangles, 3*sizeof(quint16)*nface);
		else
			cout << "Failed cache optimization" << endl;
		delete []triangles;
		start = end;
	}
}

/* extracts vertices in origin which intersects destination box */
void NexusBuilder::appendBorderVertices(uint32_t origin, uint32_t destination, std::vector<NVertex> &vertices) {
	Node &node = nodes[origin];
	uint32_t chunk = node.offset; //chunk index was stored here.


	//if origin != destination do not flush cache, it would invalidate pointers.
	uchar *buffer = chunks.getChunk(chunk, origin != destination);

	vcg::Point3f *point = (vcg::Point3f *)buffer;
	int size = sizeof(vcg::Point3f) + header.signature.vertex.hasTextures()*sizeof(vcg::Point2f);
	vcg::Point3s *normal = (vcg::Point3s *)(buffer + size * node.nvert);
	uint16_t *face = (uint16_t *)(buffer + header.signature.vertex.size()*node.nvert);

	NodeBox &nodebox = boxes[origin];
	
	vector<bool> border = nodebox.markBorders(node, point, face);
	for(int i = 0; i < node.nvert; i++) {
		if(border[i])
			vertices.push_back(NVertex(origin, i, point[i], normal + i));
	}
}


void NexusBuilder::uniformNormals() {
	cout << "Unifying normals\n";
	/*
	level 0: for each node in the lowest level:
			load the neighboroughs
			find common vertices (use lock to find the vertices)
			average normals

	level > 0: for every other node (goind backwards)
			load child nodes
			find common vertices (use lock, it's way faster!)
			copy normal
	*/

	std::vector<NVertex> vertices;

	uint32_t sink = nodes.size()-1;
	for(int t = sink-1; t > 0; t--) {
		Node &target = nodes[t];

		vcg::Box3f box = boxes[t].box;
		//box.Offset(box.Diag()/100);
		box.Offset(box.Diag()/10);

		vertices.clear();
		appendBorderVertices(t, t, vertices);

		bool last_level = (patches[target.first_patch].node == sink);

		if(last_level) {//find neighboroughs among the same level

			for(int n = t-1; n >= 0; n--) {
				Node &node = nodes[n];
				if(patches[node.first_patch].node != sink) continue;
				if(!box.Collide(boxes[n].box)) continue;

				appendBorderVertices(n, t, vertices);
			}

		} else { //again among childrens.

			for(uint p = target.first_patch; p < target.last_patch(); p++) {
				uint n = patches[p].node;

				appendBorderVertices(n, t, vertices);
			}
		}

		if(!vertices.size()) { //this is possible, there might be no border at all.
			continue;
		}

		sort(vertices.begin(), vertices.end());

		uint start = 0;
		while(start < vertices.size()) {
			NVertex &v = vertices[start];
			
			uint last = start+1;
			while(last < vertices.size() && vertices[last].point == v.point)
				last++;

			if(last_level && last - start > 1) { //average all normals
				vcg::Point3f normalf(0, 0, 0);
				for(uint k = start; k < last; k++) {
					for(int l = 0; l < 3; l++)
						normalf[l] += (*vertices[k].normal)[l];
				}
				normalf.Normalize();
				//convert back to shorts
				vcg::Point3s normals;
				for(int l = 0; l < 3; l++)
					normals[l] = (short)(normalf[l]*32766);

				for(uint k = start; k < last; k++)
					*vertices[k].normal = normals;
				
			} else //just copy from first one (coming from lower level due to sorting
				for(uint k = start; k < last; k++)
					*vertices[k].normal =*v.normal;
			
			start = last;
		}

		
		/*		for(uint k = 0; k < vertices.size(); k++) {
			NVertex &v = vertices[k];
			if(v.point != previous) {
				//uniform normals;
				if(k - last > 1) {   //more than 1 vertex to unify

					if(last_level) {     //average all normals of the coincident points.

						vcg::Point3f normalf(0, 0, 0);

						for(uint j = last; j < k; j++)
							for(int l = 0; l < 3; l++)
								normalf[l] += (*vertices[j].normal)[l];
						normalf.Normalize();

						//convert back to shorts
						vcg::Point3s normals;
						for(int l = 0; l < 3; l++)
							normals[l] = (short)(normalf[l]*32766);

						for(uint j = last; j < k; j++)
							*vertices[j].normal = normals;

					} else { //just copy the first one (it's from the lowest level, due to sort)
						if(vertices[k-1].node == t) //unless this vertex do not belongs to t.
							*vertices[k-1].normal = *vertices[last].normal;
					}
				}
				previous = v.point;
				last = k;
			}
		}*/
	}
}
