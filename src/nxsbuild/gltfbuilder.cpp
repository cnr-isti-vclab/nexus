#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
// #define TINYGLTF_NOEXCEPTION // optional. disable exception handling.

#include "gltfbuilder.h"
#include "b3dm.h"
#include <fstream>
#include <limits>
#include <unordered_map>
#include <QDir>

using namespace std;

void createBufferAndAccessor(tinygltf::Model &modelGltf,
							 void *destBuffer,
							 const void *sourceBuffer,
							 size_t bufferIndex,
							 size_t bufferViewOffset,
							 size_t bufferViewLength,
							 int bufferViewTarget,
							 size_t accessorComponentCount,
							 int accessorComponentType,
							 int accessorType)
{
	std::memcpy(destBuffer, sourceBuffer, bufferViewLength);

	tinygltf::BufferView bufferViewGltf;
	bufferViewGltf.buffer = static_cast<int>(bufferIndex);
	bufferViewGltf.byteOffset = bufferViewOffset;
	bufferViewGltf.byteLength = bufferViewLength;
	bufferViewGltf.target = bufferViewTarget;

	tinygltf::Accessor accessorGltf;
	accessorGltf.bufferView = static_cast<int>(modelGltf.bufferViews.size());
	accessorGltf.byteOffset = 0;
	accessorGltf.count = accessorComponentCount;
	accessorGltf.componentType = accessorComponentType;
	accessorGltf.type = accessorType;

	modelGltf.bufferViews.emplace_back(bufferViewGltf);
	modelGltf.accessors.emplace_back(accessorGltf);
}

void createGltfTexture(const Gltf::Texture &texture,
					   tinygltf::Model &gltf,
					   std::unordered_map<tinygltf::Sampler, unsigned> *samplerCache)
{
	tinygltf::Sampler sampler;
	sampler.minFilter = texture.minFilter;
	sampler.magFilter = texture.magFilter;
	int samplerIndex = -1;
	gltf.samplers.emplace_back(sampler);
	samplerIndex = static_cast<int>(gltf.samplers.size() - 1);
	tinygltf::Texture textureGltf;
	textureGltf.sampler = samplerIndex;
	textureGltf.source = texture.source;
	gltf.textures.emplace_back(textureGltf);
}

void createGltfMaterial(tinygltf::Model &gltf, const Gltf::Material &material) {

	vcg::Point3f specularColor = material.specular;

	float specularIntensity = specularColor[0] * 0.2125f
							  + specularColor[1] * 0.7154f
							  + specularColor[2] * 0.0721f;

	float roughnessFactor = material.shininess;
	roughnessFactor = material.shininess / 1000.0f;
	roughnessFactor = 1.0f - roughnessFactor;

	if(roughnessFactor < 0)
		roughnessFactor = 0;
	else if (roughnessFactor > 1)
		roughnessFactor = 1;
	if (specularIntensity < 0.0)
		roughnessFactor *= (1.0f - specularIntensity);

	tinygltf::Material materialGltf;
	if (material.texture != -1) {

		tinygltf::TextureInfo baseColorTexture;
		baseColorTexture.index = material.texture;

		// KHR_texture_transform
		float offset_x = 0;
		float offset_y = 1;
		float scale_x = 1.f;
		float scale_y = -1.f;

		tinygltf::Value::Array offset;
		offset.push_back(tinygltf::Value(offset_x));
		offset.push_back(tinygltf::Value(offset_y));
		tinygltf::Value::Array scale;
		scale.push_back(tinygltf::Value(scale_x));
		scale.push_back(tinygltf::Value(scale_y));
		tinygltf::Value::Object v;
		v["offset"] = tinygltf::Value(offset);
		v["scale"] = tinygltf::Value(scale);

		baseColorTexture.extensions["KHR_texture_transform"] = tinygltf::Value(v);
		materialGltf.pbrMetallicRoughness.baseColorTexture = baseColorTexture;
	}

	materialGltf.doubleSided = material.doubleSided;
	materialGltf.alphaMode = "MASK";

	if (!(material.diffuse[0] == 0 && material.diffuse[1] == 0 && material.diffuse[2] == 0)) {
		materialGltf.pbrMetallicRoughness.baseColorFactor = {material.diffuse[0],
															 material.diffuse[1],
															 material.diffuse[2],
															 material.alpha};
	}
	materialGltf.pbrMetallicRoughness.roughnessFactor = roughnessFactor;
	materialGltf.pbrMetallicRoughness.metallicFactor = 0.0;

	if (material.unlit) {
		materialGltf.extensions["KHR_materials_unlit"] = {};
	}

	gltf.materials.emplace_back(materialGltf);
}

void GltfBuilder::exportNodeAsTile(int node_index) {

	clearModel();
	nx::Node &node = m_nexus.nodes[node_index];
	Gltf::Texture customTex;
	Gltf::Material customMat;

	m_model.asset.version = "2.0";

	m_model.extensionsUsed.emplace_back("KHR_texture_transform");
	if (customMat.unlit) {
		m_model.extensionsUsed.emplace_back("KHR_materials_unlit");
	}

			// create root nodes
	tinygltf::Node rootNodeGltf;
	rootNodeGltf.matrix = {1, 0, 0, 0, 0, 0, -1, 0, 0, 1, 0, 0, 0, 0, 0, 1};
	m_model.nodes.emplace_back(rootNodeGltf);
	int rootIndex = 0;

			// create geometry buffer
	size_t totalBufferSize = node.nvert * m_nexus.header.signature.vertex.size()
							 + node.nface * m_nexus.header.signature.face.size();
	tinygltf::Buffer bufferGltf;
	auto &bufferData = bufferGltf.data;
	bufferData.resize(totalBufferSize);

			// add geometry
	tinygltf::Primitive primitiveGltf;
	primitiveGltf.mode = TINYGLTF_MODE_TRIANGLES;
	primitiveGltf.material = m_model.materials.size();

	size_t offset = 0;
	size_t nextSize = 0;
	auto bufferIndex = m_model.buffers.size();

	uint32_t chunk = node.offset;
	uchar *buffer  = m_nexus.chunks.getChunk(chunk, false);

	if (node.nvert != 0) {
		vcg::Point3f *point = (vcg::Point3f *)buffer;
		nextSize = node.nvert * sizeof(vcg::Point3f);

		createBufferAndAccessor(m_model,
								bufferData.data() + offset,
								point,
								bufferIndex,
								offset,
								nextSize,
								TINYGLTF_TARGET_ARRAY_BUFFER,
								node.nvert,
								TINYGLTF_COMPONENT_TYPE_FLOAT,
								TINYGLTF_TYPE_VEC3);

		auto box = m_nexus.boxes[node_index].box;
		auto &positionsAccessor = m_model.accessors.back();
		positionsAccessor.minValues = {box.min[0], box.min[1], box.min[2]};
		positionsAccessor.maxValues = {box.max[0], box.max[1], box.max[2]};

		primitiveGltf.attributes["POSITION"] = static_cast<int>(m_model.accessors.size() - 1);
		offset += nextSize;
	}
	else throw QString("No mesh data");


	if(m_nexus.header.signature.vertex.hasTextures()) {
		size_t uvOffset = sizeof(vcg::Point3f) * node.nvert;
		vcg::Point2f *uv = (vcg::Point2f *)(buffer + uvOffset);
		nextSize = node.nvert * sizeof(vcg::Point2f);

		createBufferAndAccessor(m_model,
								bufferData.data() + offset,
								uv,
								bufferIndex,
								offset,
								nextSize,
								TINYGLTF_TARGET_ARRAY_BUFFER,
								node.nvert,
								TINYGLTF_COMPONENT_TYPE_FLOAT,
								TINYGLTF_TYPE_VEC2);
		primitiveGltf.attributes["TEXCOORD_0"] = static_cast<int>(m_model.accessors.size() - 1);
		offset += nextSize;

//#define DEBUG_UV
#ifdef DEBUG_UV
		QFile debugUV;
		for(int i = 0; i < node.nvert; i++)
		{
			vcg::Point2f UV = *(uv + i);
			float x = UV.X();
			float y = UV.Y();
		}
#endif
	}

	if(m_nexus.header.signature.vertex.hasNormals()) {
		size_t normalOffset = sizeof(vcg::Point3f) + m_nexus.header.signature.vertex.hasTextures() * sizeof(vcg::Point2f);
		vcg::Point3s *normal = (vcg::Point3s *)(buffer + normalOffset * node.nvert);
				// TODO
	}

	if(m_nexus.header.signature.vertex.hasColors()) {
		size_t vertexColorOffset = sizeof(vcg::Point3f) + m_nexus.header.signature.vertex.hasTextures() * sizeof(vcg::Point2f)
								   + m_nexus.header.signature.vertex.hasNormals() * sizeof(vcg::Point3s);
		uchar *color =  buffer + vertexColorOffset;
				// TODO
	}

	if (node.nface != 0) {
		size_t facesOffset = m_nexus.header.signature.vertex.size() * node.nvert;
		uint16_t *face = (uint16_t *)(buffer + facesOffset);

		nextSize = node.nface * 3 * sizeof(uint16_t);
		createBufferAndAccessor(m_model,
								bufferData.data() + offset,
								face,
								bufferIndex,
								offset,
								nextSize,
								TINYGLTF_TARGET_ELEMENT_ARRAY_BUFFER,
								node.nface * 3,
								TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT,
								TINYGLTF_TYPE_SCALAR);

		primitiveGltf.indices = static_cast<int>(m_model.accessors.size() - 1);
	}

			// add buffer to gltf model
	m_model.buffers.emplace_back(bufferGltf);

			// add image data to a second buffer
			// then add an image and a texture to gltf model
	if(m_nexus.textures.size()) {

		if(m_nexus.useNodeTex) {

			int indice = m_nexus.patches[node.first_patch].texture;
			quint32 s = m_nexus.textures[indice].offset * NEXUS_PADDING;
			quint32 size = m_nexus.textures[indice + 1].offset * NEXUS_PADDING - s;
			m_nexus.nodeTex.seek(s);
			auto buffer = m_nexus.nodeTex.read(size);

			tinygltf::Buffer buffer_builder;
			auto &imageBufferData = buffer_builder.data;
			imageBufferData.resize(size);
			std::memcpy(imageBufferData.data(), buffer, size);
			auto bufferIndex = m_model.buffers.size();
			m_model.buffers.emplace_back(buffer_builder);

			tinygltf::BufferView bufferView_builder;
			bufferView_builder.buffer = bufferIndex;
			bufferView_builder.byteLength = size;
			auto imageBufferViewIndex = (int)m_model.bufferViews.size();
			m_model.bufferViews.push_back(bufferView_builder);
			tinygltf::Image img;
			img.mimeType = "image/jpeg";
			img.bufferView = imageBufferViewIndex;
			int imageIndex = m_model.images.size();
			m_model.images.emplace_back(img);
			customTex.source = imageIndex;

			createGltfTexture(customTex, m_model, nullptr);
			auto textureIndex = m_model.textures.size() - 1;
			customMat.texture = static_cast<int>(textureIndex);
//#define DEBUG_TEXTURE
#ifdef DEBUG_TEXTURE
			QString output("output");
			QString debug("output/debug");
			QString testFolder("output/debug/textures");
			QDir dir;
			dir.mkdir(output);
			dir.mkdir(debug);
			dir.mkdir(testFolder);

			QString texfileName = QString("%1/%2.jpg").arg(testFolder).arg(node_index);
			QFile texfile(texfileName);
			if(!texfile.open(QFile::WriteOnly)) {
				throw QString("Error while opening %1").arg(texfileName);
			}
			bool success = texfile.write(buffer, size);
			if(!success)
				throw QString("failed writing texture data from temporary file.");
			texfile.close();
#endif
		}
		else { /* keep original texture : TODO*/ }
	}

			// add material
	createGltfMaterial(m_model, customMat);

			// add mesh
	tinygltf::Mesh meshGltf;
	meshGltf.primitives.emplace_back(primitiveGltf);
	m_model.meshes.emplace_back(meshGltf);

			// create node
	tinygltf::Node meshNode;
	meshNode.mesh = static_cast<int>(m_model.meshes.size() - 1);
	//meshNode.translation = {center.x, center.y, center.z};
	m_model.nodes.emplace_back(meshNode);

			// add node to the root
	m_model.nodes[rootIndex].children.emplace_back(m_model.nodes.size() - 1);

			// create scene
	tinygltf::Scene sceneGltf;
	sceneGltf.nodes.emplace_back(0);
	m_model.scenes.emplace_back(sceneGltf);

	QString filename = QString("%1.b3dm").arg(node_index);
	QString filename2 = QString("%1.glb").arg(node_index);
	QString folder("output");

	QDir dir;
	dir.mkdir(folder);
	QString path = QString("%1/%2").arg(folder).arg(filename);
	writeB3DM(path);

//#define DEBUG_GLTF
#ifdef DEBUG_GLTF
	QString debug_folder("output/debug");
	dir.mkdir(debug_folder);
	QString debug_path = QString("%1/%2").arg(debug_folder).arg(filename2);
	writeGLB(debug_path);
#endif  
}

void GltfBuilder::writeGLB(const QString &path) {
	tinygltf::TinyGLTF loader;
	loader.WriteGltfSceneToFile(&m_model, path.toStdString(), true, true, true, true);
}

void GltfBuilder::writeB3DM(const QString & path) {
	tinygltf::TinyGLTF loader;

			// writing .glb file to output/temp%
	QString tempPath = QString("output/temp");
	bool result = loader.WriteGltfSceneToFile(&m_model, tempPath.toStdString(), true, true, true, true);
	if(!result) { throw QString("Error : can't write gltf to file : output/temp"); }

			// looking for file size
	std::ifstream is(tempPath.toStdString(), std::ifstream::in|std::ifstream::binary);
	is.ignore(std::numeric_limits<std::streamsize>::max());
	std::streamsize length = is.gcount();
	is.clear();
	is.seekg( 0, std::ifstream::beg);
	char *buffer = new char[length];
	is.read(buffer, length);
	is.close();
	b3dm b3dmFile(length);
	std::filebuf fb;
	fb.open(path.toStdString(), std::ios::out|std::ios::binary);
	std::ostream os(&fb);
	b3dmFile.writeToStream(os);
	os.write(buffer, length);
	os.write(b3dmFile.getEndPadding().c_str(), std::streamsize(b3dmFile.getEndPadding().size()));
	fb.close();
	QFile::remove(tempPath);

	delete[] buffer;
}

void GltfBuilder::generateTiles() {
	for (uint i = 0; i < m_nexus.nodes.size() - 1; i++)
		exportNodeAsTile(i);
}

