#include "export_nxs.h"
#include "../core/mesh_hierarchy.h"
#include "../common/dag.h"
#include "../core/log.h"
#include <fstream>
#include <set>
#include <map>
#include <stdio.h>
#include <cstring>
#include <QImage>
#include <QImageWriter>
#include <QBuffer>
#include <QByteArray>

/*
NXS file save logic (legacy reference):
 - Write header struct (with counts, version, etc)
 - Write node array (Node structs)
 - Write patch array (Patch structs)
 - (Textures: skip for now)
 - For each node, write geometry chunk (buffered triangle/vertex data)
 - (Textures: skip)
 - Finalize file

For our stub/first version:
 - Only header, nodes, patches, and geometry chunks are needed.
 - Geometry chunk layout and struct definitions must match NXS spec.
 - See legacy NexusBuilder::save for offset/padding logic.
*/

namespace nx {

void ExportNxs::export_nxs(MeshHierarchy& hierarchy, const std::string& path) {

	out = fopen(path.c_str(), "wb");
	if (!out) {
		throw std::runtime_error("ExportNxs: failed to open output file: " + path);

	}

	header.version = 2;

	Signature &signature = header.signature;
	signature.vertex.setComponent(VertexElement::COORD, Attribute(Attribute::FLOAT, 3));
	signature.face.setComponent(FaceElement::INDEX, Attribute(Attribute::UNSIGNED_SHORT, 3));
	//if(components & NORMALS)
	signature.vertex.setComponent(VertexElement::NORM, Attribute(Attribute::SHORT, 3));
	/*if(components & COLORS)
		signature.vertex.setComponent(VertexElement::COLOR, Attribute(Attribute::BYTE, 4)); */
	if(hierarchy.levels[0]->has_textures)
		signature.vertex.setComponent(FaceElement::TEX, Attribute(Attribute::FLOAT, 2));

	header.nvert = header.nface = header.n_nodes = header.n_patches = header.n_textures = 0;
	header.version = 2;
	nodes.clear();
	patches.clear();
	textures.clear();
	texture_payloads.clear();

	//temporarily write header to disk



	level_node_offset.resize(hierarchy.levels.size());
	//count nodes and patchs.
	for(int level = hierarchy.levels.size()-1; level >= 0; level--) {
		level_node_offset[level] = header.n_nodes;
		const MappedMesh &mesh = *hierarchy.levels[level];
		header.n_nodes += mesh.micronodes.size();
		header.n_patches += mesh.clusters.size();
	}
	if(signature.vertex.hasTextures())
		header.n_textures = header.n_nodes + 1; // +1 sentinel texture entry
	header.n_nodes++; //sink

	//temporarily write nodes, we will come back after we have the numbers.
	size_t index_size = sizeof(Header) + sizeof(Node)*header.n_nodes + sizeof(Patch)*header.n_patches + sizeof(Texture)*header.n_textures;
	pad(index_size);
	fseek(out, index_size, SEEK_SET);

	//write the nodes to disk
	for(int level = hierarchy.levels.size()-1; level >= 0; level--) {
		MappedMesh &mesh = *hierarchy.levels[level];
		for(size_t m = 0; m < mesh.micronodes.size(); m++) {
			exportMicronode(level, static_cast<Index>(m), mesh, out);
		}
	}

	if(signature.vertex.hasTextures()) {
		assert(textures.size() == texture_payloads.size());
		for(size_t i = 0; i < texture_payloads.size(); ++i) {
			alignFile();
			textures[i].offset = static_cast<uint32_t>(ftell(out) / NEXUS_PADDING);
			const std::vector<uint8_t>& payload = texture_payloads[i];
			if(!payload.empty())
				fwrite(payload.data(), 1, payload.size(), out);
		}
		alignFile();
		Texture sentinel;
		sentinel.offset = static_cast<uint32_t>(ftell(out) / NEXUS_PADDING);
		textures.push_back(sentinel);
		assert(textures.size() == header.n_textures);
	}

	for(Patch &patch: patches)
		if(patch.node == 0xFFFFFFFF)
			patch.node = nodes.size(); //sink

	Node sink;
	sink.offset = ftell(out)/NEXUS_PADDING;
	sink.first_patch = patches.size();
	nodes.push_back(sink);

	assert(nodes.size() == header.n_nodes);



	saturateNodes();
	printDag(hierarchy);
	//printDagHierarchy(hierarchy);

	//finish header:
	header.nvert = hierarchy.levels[0]->wedges.size();
	header.nface = hierarchy.levels[0]->triangles.size();
	header.n_nodes = nodes.size();
	header.n_patches = patches.size();
	header.n_textures = textures.size();
	vcg::Point3f center(nodes[0].sphere.Center());
	header.sphere = vcg::Sphere3f(center, nodes[0].tight_radius);

	fseek(out, 0, SEEK_SET);
	fwrite((char *)&header, sizeof(Header), 1, out);
	fwrite((char*)&(nodes[0]), 1, sizeof(Node)*nodes.size(), out);
	fwrite((char*)&(patches[0]), 1, sizeof(Patch)*patches.size(), out);
	if(!textures.empty())
		fwrite((char*)&(textures[0]), 1, sizeof(Texture)*textures.size(), out);

	fclose(out);
}

std::vector<uint8_t> ExportNxs::encodeBaseColorJpeg(const MappedMesh& mesh, uint32_t micronode_id, int quality) {
	assert(mesh.has_textures);
	assert(micronode_id < mesh.node_textures.size());
	const NodeTexture& node_texture = mesh.node_textures[micronode_id];
	assert(node_texture.width > 0 && node_texture.height > 0);
	assert(node_texture.components == 3);

	const std::size_t pixel_count = static_cast<std::size_t>(node_texture.width) * static_cast<std::size_t>(node_texture.height);
	const std::size_t base_slot_bytes = pixel_count * 3;
	assert(mesh.texels.size() >= node_texture.offset + base_slot_bytes);

	const uint8_t* src = mesh.texels.data() + node_texture.offset;
	assert(node_texture.offset + node_texture.width*node_texture.height*3 <= mesh.texels.size());

	QImage image(node_texture.width, node_texture.height, QImage::Format_RGB888);
	for(int y = 0; y < node_texture.height; ++y) {
		uint8_t* dst_row = image.scanLine(node_texture.height - y -1);
		const std::size_t row_offset = static_cast<std::size_t>(y) * static_cast<std::size_t>(node_texture.width) * 3;
		std::memcpy(dst_row, src + row_offset, static_cast<std::size_t>(node_texture.width) * 3);
	}

	QByteArray encoded;
	QBuffer buffer(&encoded);
	bool opened = buffer.open(QIODevice::WriteOnly);
	assert(opened);
	QImageWriter writer(&buffer, "jpg");
	writer.setQuality(quality);
#if QT_VERSION >= QT_VERSION_CHECK(5, 5, 0)
	writer.setOptimizedWrite(true);
	writer.setProgressiveScanWrite(true);
#endif
	bool ok = writer.write(image);
	assert(ok);
	buffer.close();

	return std::vector<uint8_t>(encoded.begin(), encoded.end());
}

void ExportNxs::printDag(MeshHierarchy &hierarchy) {
	nx::debug << "DAG (levels 0.." << (hierarchy.levels.size() ? (hierarchy.levels.size() - 1) : 0) << ")"
			  << std::endl;

	nx::debug << "Nodes: " << nodes.size() << ", Patches: " << patches.size() << std::endl;

	for (size_t i = 0; i < nodes.size()-1; ++i) {
		nx::Node &node = nodes[i];
		nx::debug << "Node " << i
				  << " nvert=" << node.nvert
				  << " nface=" << node.nface
				  << " error=" << node.error
				  << " first_patch=" << node.first_patch
				  << " last_patch=" << node.last_patch()
				  << std::endl;

		for (uint32_t p = node.first_patch; p < node.last_patch(); ++p) {
			const nx::Patch &patch = patches[p];
			nx::debug << "  Patch " << p
					  << " -> node " << patch.node
					  << std::endl;
		}
	}
}

void ExportNxs::printDagHierarchy(MeshHierarchy &hierarchy) {
	nx::debug << "DAG (hierarchy micronodes/clusters)" << std::endl;
	for (int level = hierarchy.levels.size()- 1; level >= 0; --level) {
		MappedMesh &mesh = *hierarchy.levels[level];
		nx::debug << "Level " << level
				  << " | micronodes=" << mesh.micronodes.size()
				  << " | clusters=" << mesh.clusters.size()
				  << std::endl;

		for (size_t m = 0; m < mesh.micronodes.size(); ++m) {
			MicroNode &mn = mesh.micronodes[m];
			nx::debug << "  Node " << m
					  << " nvert=" << mn.vertex_count
					  << " nface=" << mn.triangle_count
					  << " error=" << mn.error
					  << std::endl;
			nx::debug << "    patches=[";
			for (size_t i = 0; i < mn.cluster_ids.size(); ++i) {
				Index cid = mn.cluster_ids[i];
				Cluster &cluster = mesh.clusters[cid];
				nx::debug << cid << " -> " << cluster.node;
				if (i + 1 < mn.cluster_ids.size()) nx::debug << ", ";
			}
			nx::debug << "]";

			nx::debug << std::endl;
		}
	}
}



void ExportNxs::exportMicronode(int level, std::uint32_t micronode_id, MappedMesh &mesh, FILE *out) {
	MicroNode &micronode = mesh.micronodes[micronode_id];

	nx::Node node;
	node.first_patch = patches.size();
	Index texture_index = NONE;
	if(mesh.has_textures) {
		texture_index = static_cast<Index>(textures.size());
		textures.push_back(Texture());
		texture_payloads.push_back(encodeBaseColorJpeg(mesh, micronode_id));
	}

	std::map<Index, Index> wedges_ids; //this are the local wedges.
	size_t n_triangles = 0;
	int count = 0;
	for(Index c: micronode.cluster_ids) {
		const Cluster &cluster = mesh.clusters[c];
		n_triangles += cluster.triangle_count;
		size_t end = cluster.triangle_offset + cluster.triangle_count;
		for(size_t t = cluster.triangle_offset; t < end; t++) {
			assert(t < mesh.triangles.size());
			const Triangle &tri = mesh.triangles[t];
			for(int k = 0; k < 3; k++) {
				if(!wedges_ids.count(tri.w[k]))
					wedges_ids[tri.w[k]] = count++;
			}
		}
	}
	size_t n_wedges = wedges_ids.size();
	size_t size = n_triangles*header.signature.face.size() +
				  n_wedges*header.signature.vertex.size();
	pad(size);
	char *buffer = new char[size];

	Vector3f *vertices = (Vector3f *)buffer;
	Vector2f *texcoords = nullptr;
	int16_t *normals = (int16_t *)(buffer + 12*n_wedges);

	if(mesh.has_textures) {
		texcoords = (Vector2f *)(buffer + 12*n_wedges);
		normals = (int16_t *)(buffer + 20*n_wedges);
	}

	//write the wedges
	for(const auto [old_index, new_index]: wedges_ids) {
		const Wedge &wedge = mesh.wedges[old_index];
		vertices[new_index] = mesh.positions[wedge.p];
		Vector3f n{0, 0, 0};
		if(mesh.has_normals && wedge.n != NONE) {
			n = mesh.normals[wedge.n];
		}
		normals[new_index*3 + 0] = int16_t(n.x*32767);
		normals[new_index*3 + 1] = int16_t(n.y*32767);
		normals[new_index*3 + 2] = int16_t(n.z*32767);
		if(mesh.has_textures) {
			texcoords[new_index] = mesh.texcoords[wedge.t];
		}
	}

	uint16_t *triangles = (uint16_t *)(buffer + header.signature.vertex.size()*n_wedges);
	int tri_count = 0;
	for(Index c = 0; c < micronode.cluster_ids.size(); c++) {
		const Cluster &cluster = mesh.clusters[micronode.cluster_ids[c]];
		nx::Patch patch;
		if(cluster.node == NONE) {
			patch.node = header.n_nodes-1; //first level, sink node
			assert(level == 0);
		} else {
			patch.node = cluster.node + level_node_offset[level-1];
		}
		patch.texture = mesh.has_textures ? texture_index : NONE;

		size_t end = cluster.triangle_offset + cluster.triangle_count;
		for(size_t t = cluster.triangle_offset; t < end; t++) {
			const Triangle &tri = mesh.triangles[t];
			for(int k = 0; k < 3; k++) {
				*triangles = wedges_ids[tri.w[k]];
				triangles++;
			}
			tri_count++;
		}
		patch.triangle_offset = tri_count;
		patches.push_back(patch);
	}

	node.nvert = n_wedges;
	node.nface = n_triangles;
	node.offset = ftell(out)/NEXUS_PADDING;
	node.error = micronode.error;

	vcg::Point3f center(micronode.center.x, micronode.center.y, micronode.center.z);
	node.sphere = vcg::Sphere3f(center, micronode.radius);
	node.tight_radius = micronode.radius;

	nodes.push_back(node);

	fwrite(buffer, 1, size, out);
	//TODO we might want to optimize the node by cache coherence (or compress!)
}


void ExportNxs::saturateNodes() {
	//assumption: the nodes are ordered such that child comes always after parent
	for(int node = nodes.size()-2; node >= 0; node--)
		saturateNode(node);

	nodes.back().error = 0;
}

//include sphere of the children and ensure error s bigger.
void ExportNxs::saturateNode(uint32_t n) {
	const float epsilon = 1.01f;

	nx::Node &node = nodes[n];
	for(uint32_t i = node.first_patch; i < node.last_patch(); i++) {
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
} // namespace nx
