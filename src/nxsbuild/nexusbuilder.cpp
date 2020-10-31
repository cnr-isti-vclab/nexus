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
#include <QThread>
#include <QFileInfo>
#include <QPainter>
#include <QImage>
#include <QImageWriter>
#include "vertex_cache_optimizer.h"

#include "nexusbuilder.h"
#include "kdtree.h"
#include "meshstream.h"
#include "mesh.h"
#include "tmesh.h"
#include "../common/nexus.h"

#include <vcg/math/similarity2.h>
#include <vcg/space/rect_packer.h>

#include <iostream>
using namespace std;

using namespace nx;



/*unsigned int nextPowerOf2 (unsigned int n) {
    unsigned count = 0;

    if (n && !(n & (n - 1)))
        return n;

    while( n != 0)
    {
        n >>= 1;
        count += 1;
    }
    return 1 << count;
}*/


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
	signature.vertex.setComponent(VertexElement::POSITION, Attribute(Attribute::FLOAT, 3));
	if(components & FACES)     //ignore normals for meshes
		signature.face.setComponent(FaceElement::INDEX, Attribute(Attribute::UNSIGNED_SHORT, 3));
	if(components & NORMALS)
		signature.vertex.setComponent(VertexElement::NORM, Attribute(Attribute::SHORT, 3));
	if(components & COLORS)
		signature.vertex.setComponent(VertexElement::COLOR, Attribute(Attribute::BYTE, 4));
	if(components & TEXTURES)
		signature.vertex.setComponent(FaceElement::TEX, Attribute(Attribute::FLOAT, 2));

	header.version = 3;
	header.signature = signature;
	header.nvert = header.nface = header.n_nodes = header.n_patches = header.n_textures = 0;

	nodeTex.open();
}

NexusBuilder::NexusBuilder(Signature &signature): chunks("cache_chunks"), scaling(0.5) {
	header.version = 3;
	header.signature = signature;
	header.nvert = header.nface = header.n_nodes = header.n_patches = header.n_textures = 0;
}

bool NexusBuilder::initAtlas() {
	for(auto &material: materials) {
		bool success = atlas.addTextures(material.textures, material.flipY);
		if(!success)
			return false;
	}
	nodeTexCreator.atlas = &atlas;
	nodeTexCreator.materials = &materials;
	nodeTexCreator.createPowTwoTex = createPowTwoTex;
	return true;
}

void NexusBuilder::create(KDTree *tree, Stream *stream, uint top_node_size) {
	Node sink;
	sink.first_patch = 0;
	nodes.push_back(sink);

	int level = 0;
	int last_top_level_size = 0;
	QElapsedTimer timer;
	timer.start();
	do {
		cout << "Creating level " << level << endl;
		tree->clear();
		if(level % 2) tree->setAxesDiagonal();
		else tree->setAxesOrthogonal();

		tree->load(stream);
		cout << "Streaming " << timer.restart()/1000.0f << endl;
		stream->clear();

		createLevel(tree, stream, level);
		cout << "Simplifying " << timer.restart()/1000.0f << endl;

		level++;
		if(skipSimplifyLevels <= 0 && last_top_level_size != 0 && stream->size()/(float)last_top_level_size > 0.7f) {
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

/*
  Commented because the gain is negligible ( and the code is not correct either, there is
  some problem in saving the triangles... */


class Worker: public QThread {
public:
	uint block;
	KDTree &input;
	StreamSoup &output;
	NexusBuilder &builder;

	Worker(uint n, KDTree &in, StreamSoup &out, NexusBuilder &parent):
		block(n), input(in), output(out), builder(parent) {}

protected:
	void run() {
		Mesh mesh;
		Soup soup;
		{
			QMutexLocker locker(&builder.m_input);
			soup = input->getSoup(block, true);
		}
		mesh.load(soup);
		input->lock(mesh, bbufferlock);

		{
			//QMutexLocker locker(&builder.m_input);
			input->dropSoup(block);
		}

		quint32 patch_offset = 0;
		quint32 chunk = 0;
		{
			QMutexLocker output(&builder.m_chunks);

			//save node in nexus temporary structure
			quint32 mesh_size = mesh.serializedSize(builder.header.signature);
			mesh_size = builder.pad(mesh_size);
			chunk = builder.chunks.addChunk(mesh_size);
			uchar *buffer = builder.chunks.getChunk(chunk);
			patch_offset = builder.patches.size();
			std::vector<Patch> node_patches;
			mesh.serialize(buffer, builder.header.signature, node_patches);

			//patches will be reverted later, but the local order is important because of triangle_offset
			std::reverse(node_patches.begin(), node_patches.end());
			builder.patches.insert(builder.patches.end(), node_patches.begin(), node_patches.end());
		}

		float error = mesh.simplify(mesh.fn/2, Mesh::QUADRICS);

		quint32 current_node = 0;
		{
			QMutexLocker locker(&builder.m_builder);

			current_node = builder.nodes.size();
			nx::Node node = mesh.getNode();
			node.offset = chunk; //temporaryle remember which chunk belongs to which node
			node.error = error;
			node.first_patch = patch_offset;
			builder.nodes.push_back(node);
		}

		{
			QMutexLocker locker(&builder.m_output);
			Triangle *triangles = new Triangle[mesh.fn];
			//streaming the output TODO be sure not use to much memory on the chunks: we are writing sequentially
			mesh.getTriangles(triangles, current_node);
			for(int i = 0; i < mesh.fn; i++)
				output.pushTriangle(triangles[i]);

			delete []triangles;
		}

	}
};



void NexusBuilder::createCloudLevel(KDTreeCloud *input, StreamCloud *output, int /*level*/) {

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
			node.size = mesh_size;
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

void NexusBuilder::createMeshLevel(KDTreeSoup *input, StreamSoup *output, int level) {
		atlas.buildLevel(level);
		if(level > 0)
			atlas.flush(level-1);

		//const int n_threads = 3;
		//QList<Worker *> workers;

		double area = 0.0;

		QElapsedTimer timer;
		timer.start();

		int64_t io_time = 0;
		int64_t packing_time = 0;
		int64_t simplify_time = 0;

		for(uint block = 0; block < input->nBlocks(); block++) {

			/*if(workers.size() > n_threads) {
			workers.front()->wait();
			delete workers.front();
			workers.pop_front();
		}
		Worker *worker = new Worker(block, input, output, *this);
		worker->start();
		workers.push_back(worker);
		//worker->wait();
		*/

			Soup soup = input->get(block);
			assert(soup.size() < (1<<16));
			if(soup.size() == 0) continue;
			
			/*sequence of operations:
			1) load from soup
			2) lock			
			3) if(textures) make a copy and split the seams.
			4) compute serialize size
			5) allocate the chunk.
			6) if(texture) create the textures and save in temporary file. (we could skip and save right away
			7) serialize to chunk
			8) get node data 
			10) simplify and compute error 
			11) finalize the node (we need the error)
			12) stream the triangles
			*/

			TMesh mesh;
			TMesh tmp; //this is needed saving a mesh with vertices on seams duplicated., and for node tex coordinates to be rearranged

			Mesh mesh1;
			quint32 mesh_size;

			if(!hasTextures()) {
				mesh1.load(soup);

				input->lock(mesh1, block);
				mesh_size = mesh1.serializedSize(header.signature);
				
			} else {
				
				mesh.load(soup);
				input->lock(mesh, block);
				//we need to replicate vertices where textured seams occours

				vcg::tri::Append<TMesh,TMesh>::MeshCopy(tmp,mesh);
				for(int i = 0; i < tmp.face.size(); i++) {
					tmp.face[i].node = mesh.face[i].node;
					tmp.face[i].tex = mesh.face[i].tex;
				}
				tmp.splitSeams(header.signature);
				//TODO don't exit, just bail out!
				if(tmp.vert.size() > 60000) {
					cerr << "Unable to properly simplify due to framented parametrization\n"
						 << "Try to reduce the size of the nodes using -f (default is 32768)" << endl;
					exit(0);
				}

				//save node in nexus temporary structure
				mesh_size = tmp.serializedSize(header.signature);
			}
			mesh_size = pad(mesh_size);
			quint32 chunk = chunks.addChunk(mesh_size);
			uchar *buffer = chunks.getChunk(chunk);
			quint32 patch_offset = patches.size();
			std::vector<Patch> node_patches;

			float error;
			if(!hasTextures()) {
				mesh1.serialize(buffer, header.signature, node_patches);

			} else {
				io_time += timer.restart();
				TextureGroupBuild group = nodeTexCreator.process(tmp, level);
				error = group.error;

				//TODO area!!
				//QImage nodetex = extractNodeTex(tmp, level, error, pixelXedge);
				//area += nodetex.width()*nodetex.height();
				TextureGroup texture;
				texture.offset = nodeTex.size()/NEXUS_PADDING;
				int32_t nimages = group.size();
				nodeTex.write((char *)&nimages, 4);
				int64_t	pos = nodeTex.size();
				for(QImage nodetex: group) {
					output_pixels += nodetex.width()*nodetex.height();
					//reserve space for image size, will be overwritten.
					nodeTex.write((char *)&texture.size, 4);
					QImageWriter writer(&nodeTex, "jpg");
					writer.setQuality(tex_quality);
#if QT_VERSION >= QT_VERSION_CHECK(5, 5, 0)
					writer.setOptimizedWrite(true);
					writer.setProgressiveScanWrite(true);
#endif
					writer.write(nodetex);

//#define SAVE_NODE_MESH
#ifdef  SAVE_NODE_MESH
					static int counter = 0;
					QString texname = QString::number(counter) + ".jpg";
					nodetex.save(texname);
					tmp.textures.push_back(texname.toStdString());
					tmp.saveObjTex(QString::number(counter) + ".obj", texname);
					counter++;
#endif
					//No padding needed!
					//qint64 file_end = pad(nodeTex.size());
					int64_t image_end = nodeTex.size();
					int32_t tex_size = image_end - pos - 4; //pos points at the dimension int now

					nodeTex.seek(pos);
					nodeTex.write((char *)&tex_size, 4);
					nodeTex.seek(image_end);
					pos = image_end;
				}
				int64_t file_end = pad(nodeTex.size());
				nodeTex.resize(file_end);
				nodeTex.seek(file_end);

				texture.size = uint32_t(nodeTex.size() - texture.offset * NEXUS_PADDING);
				textures.push_back(texture);

				packing_time += timer.restart();

				tmp.serialize(buffer, header.signature, node_patches);
				for(Patch &patch: node_patches) {
					patch.texture = textures.size()-1;
					patch.material = group.material;
				} //last texture insertet

				//VICIOUS TRICK: we could save only a texture every 2 geometry levels since the patch is contained also to a parent node.
				//we could store the texture in the parent nodes i and have it good also for the children node.
				//but only starting from the bottom.

			}

			//patches will be reverted later, but the local order is important because of triangle_offset
			std::reverse(node_patches.begin(), node_patches.end());
			patches.insert(patches.end(), node_patches.begin(), node_patches.end());

			nx::Node node;
			if(!hasTextures())
				node = mesh1.getNode(); //get node data before simplification
			else
				node = tmp.getNode();

			io_time += timer.restart();

			int nface;
			if(!hasTextures()) {
				error = mesh1.simplify(soup.size()*scaling, Mesh::QUADRICS);
				nface = mesh1.fn;
			} else {
				
//cout << "e do not need skiplevel: qwe need to group and do not simplify increasing the number of triangles per mesh! with a max of texel per patch instead." << endl;
				int target_faces = soup.size()*scaling;
				//if(pixelXedge > 10) {
				//When textures are too big for the amount of geometry we skip some level of geometry simplification.
				//It should be automatic based on pixelXedge....
				if(skipSimplifyLevels > 0) {
					//cout << "Too much texture! Skipping vertex simplification" << endl;
					target_faces = soup.size();
				} 
				//mesh.savePly("test.ply");
				//exit(0);
				mesh.simplify(target_faces, TMesh::QUADRICS);
				nface = mesh.fn;
			}
			simplify_time += timer.restart();


			quint32 current_node = nodes.size();

			node.offset = chunk; //temporarily remember which chunk belongs to which node
			node.size = mesh_size;
			node.error = error;
			node.first_patch = patch_offset;
			nodes.push_back(node);
			boxes.push_back(NodeBox(input, block));

			Triangle *triangles = new Triangle[nface];
			//streaming the output TODO be sure not use to much memory on the chunks: we are writing sequentially
			if(!hasTextures()) {
				mesh1.getTriangles(triangles, current_node);
			} else {
				mesh.getTriangles(triangles, current_node);
			}
			for(int i = 0; i < nface; i++) {
				Triangle &t = triangles[i];
				if(!t.isDegenerate())
					output->pushTriangle(triangles[i]);
			}

			delete []triangles;

			io_time += timer.restart();

		}
		double n = input->nBlocks();
		double io = double(io_time)/n;
		double packing = double(packing_time)/n;
		double simplify = double(simplify_time)/n;

		cout << "Per node: packing: " << packing << "ms " << " Simplification: " << simplify << "ms " << " IO: " << io << "ms" << endl;

		//std::cout << "Level texture area: " << area << endl;
		/*while(workers.size()) {
		workers.front()->wait();
		delete workers.front();
		workers.pop_front();
	}*/
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

void NexusBuilder::saveGLTF(QFile &file) {

}


void NexusBuilder::save(QString filename, bool gltf) {

	file.setFileName(filename);
	if(!file.open(QIODevice::ReadWrite | QIODevice::Truncate))
		throw QString("could not open file " + filename);

	if(header.signature.vertex.hasNormals() && header.signature.face.hasIndex())
		uniformNormals();

	if(textures.size())
		textures.push_back(TextureGroup());


	// Prepare the header
	header.nface = 0;
	header.nvert = 0;
	header.n_nodes = nodes.size();
	header.n_patches = patches.size();
	header.n_textures = textures.size();
	header.version = 3;

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
	//unify materials, writing to header.materials.
	std::vector<int> material_map = materials.compact(header.materials);
	for(Patch &patch: patches)
		patch.material = material_map[patch.material];

	if(gltf)
		saveGLTF(file);
	else
		saveNXS(file);
}

void NexusBuilder::saveNXS(QFile &file) {


	vector<char> header_dump = header.write();

	quint64 size = header_dump.size()  +
			nodes.size()*sizeof(Node) +
			patches.size()*sizeof(Patch) +
			textures.size()*sizeof(TextureGroup);
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
	//TODO if notes textures are not shared, put them consecutively
	//so

	if(textures.size()) {
		if(!useNodeTex) { //texture.offset keeps the size of each texture
			for(uint i = 0; i < textures.size()-1; i++) {
				quint32 s = textures[i].offset;
				textures[i].offset = size/NEXUS_PADDING;
				size += s;
				size = pad(size);
			}
			textures.back().offset = size/NEXUS_PADDING;

		} else { //texture.offset keeps the index in the nodeTex temporay file (already padded and in NEXUS_PADDING units
			for(uint i = 0; i < textures.size()-1; i++)
				textures[i].offset += size/NEXUS_PADDING;
			size += nodeTex.size();
			textures.back().offset = size/NEXUS_PADDING;
		}
	}

	qint64 r = file.write(header_dump.data(), header_dump.size());
	if(r == -1)
		throw(file.errorString());
	assert(nodes.size());
	file.write((char*)&(nodes[0]), sizeof(Node)*nodes.size());
	if(patches.size())
		file.write((char*)&(patches[0]), sizeof(Patch)*patches.size());
	if(textures.size())
		file.write((char*)&(textures[0]), sizeof(TextureGroup)*textures.size());
	file.seek(index_size);

	//NODES
	for(uint i = 0; i < node_chunk.size(); i++) {
		quint32 chunk = node_chunk[i];
		uchar *buffer = chunks.getChunk(chunk);
		optimizeNode(i, buffer);
		file.write((char*)buffer, chunks.chunkSize(chunk));
	}

	//TEXTURES
	//	QString basename = filename.left(filename.length()-4);
	//compute textures offsetse
	if(hasTextures()) {
		if(useNodeTex) {
			//todo split into pieces.
			nodeTex.seek(0);
			bool success = file.write(nodeTex.readAll());
			if(!success)
				throw QString("failed writing texture");

		} else {
			throw "Preserving original textures not supported anymode";
/*			for(int i = 0; i < textures.size()-1; i++) {
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
			} */
		}
	}
	if(textures.size())
		for(int i = 0; i < textures.size()-1; i++) {
			QFile::remove(QString("nexus_tmp_tex%1.png").arg(i));
		}
	//cout << "Saving to file " << qPrintable(filename) << endl;
	file.close();
}

qint64 NexusBuilder::pad(qint64 s) {
	const qint64 padding = NEXUS_PADDING;
	qint64 m = (s-1) & ~(padding -1);
	return m + padding;
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
