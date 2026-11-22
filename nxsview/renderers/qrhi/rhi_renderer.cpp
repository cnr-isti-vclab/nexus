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
#include "rhi_renderer.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>
#include <utility>

#include <QByteArray>
#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QResource>
#include <rhi/qshader.h>

#include "../../nexus.h"

static void initializeRhiShaderResources() {
#ifdef NXSVIEW_QRHI_RESOURCES
	Q_INIT_RESOURCE(nxsview_qrhi_shaders);
#endif
}

using namespace nx;

namespace {

QShader readShader(const QString &resourceName) {
	QFile file(resourceName);
	if(!file.open(QIODevice::ReadOnly)) {
		const QString shaderPath = QCoreApplication::applicationDirPath()
			+ QStringLiteral("/shaders/") + QFileInfo(resourceName).fileName();
		file.setFileName(shaderPath);
		if(!file.open(QIODevice::ReadOnly)) {
			qWarning() << "Unable to open RHI shader" << resourceName << "or" << shaderPath;
			return {};
		}
	}

	QShader shader = QShader::fromSerialized(file.readAll());
	if(!shader.isValid())
		qWarning() << "Invalid RHI shader resource" << resourceName;
	return shader;
}

QColor toQColor(const Rgba8 &color) {
	return QColor(color.r, color.g, color.b, 255);
}

QMatrix4x4 textureMatrix(const float *matrix) {
	return QMatrix4x4(matrix);
}

template<typename T>
float vertexValue(const char *data, bool normalized) {
	T value;
	std::memcpy(&value, data, sizeof(value));
	if constexpr(std::is_floating_point_v<T>) {
		return static_cast<float>(value);
	} else if constexpr(std::is_signed_v<T>) {
		return normalized
			? std::max(-1.0f, static_cast<float>(value) / std::numeric_limits<T>::max())
			: static_cast<float>(value);
	} else {
		return normalized
			? static_cast<float>(value) / std::numeric_limits<T>::max()
			: static_cast<float>(value);
	}
}

bool readVertexValue(const char *vertex, const VertexElement &format,
                     unsigned int component, unsigned int element,
                     bool normalized, float &value) {
	const Attribute &attribute = format.attributes[component];
	if(attribute.isNull() || attribute.type > Attribute::DOUBLE || element >= attribute.number)
		return false;

	const char *data = vertex + format.componentOffset(component);
	switch(attribute.type) {
	case Attribute::BYTE:
		value = vertexValue<int8_t>(data + element, normalized);
		return true;
	case Attribute::UNSIGNED_BYTE:
		value = vertexValue<uint8_t>(data + element, normalized);
		return true;
	case Attribute::SHORT:
		value = vertexValue<int16_t>(data + element * sizeof(int16_t), normalized);
		return true;
	case Attribute::UNSIGNED_SHORT:
		value = vertexValue<uint16_t>(data + element * sizeof(uint16_t), normalized);
		return true;
	case Attribute::INT:
		value = vertexValue<int32_t>(data + element * sizeof(int32_t), normalized);
		return true;
	case Attribute::UNSIGNED_INT:
		value = vertexValue<uint32_t>(data + element * sizeof(uint32_t), normalized);
		return true;
	case Attribute::FLOAT:
		value = vertexValue<float>(data + element * sizeof(float), normalized);
		return true;
	case Attribute::DOUBLE:
		value = vertexValue<double>(data + element * sizeof(double), normalized);
		return true;
	default:
		return false;
	}
}

bool hasReadableAttribute(const Attribute &attribute, unsigned int components) {
	return !attribute.isNull() && attribute.type <= Attribute::DOUBLE &&
	       attribute.number >= components;
}

} // namespace

void RhiRenderer::checkRhiErrors(const char *context) {
	if(!rhi)
		return;

	if(rhi->isDeviceLost()) {
		qWarning() << "QRhi device lost detected at" << context;
	}
}

RhiRenderer::RhiRenderer() {
	setGpuCapacity(gpuCapacity);
}

RhiRenderer::~RhiRenderer() {
	releaseResources();
}

std::size_t RhiRenderer::GeometryKeyHash::operator()(const GeometryKey &key) const noexcept {
	const std::size_t pointer = std::hash<Nexus *>{}(key.nexus);
	const std::size_t node = std::hash<uint32_t>{}(key.node);
	return pointer ^ (node + 0x9e3779b9 + (pointer << 6) + (pointer >> 2));
}

std::size_t RhiRenderer::TextureKeyHash::operator()(const TextureKey &key) const noexcept {
	const std::size_t pointer = std::hash<Nexus *>{}(key.nexus);
	const std::size_t texture = std::hash<uint32_t>{}(key.texture);
	return pointer ^ (texture + 0x9e3779b9 + (pointer << 6) + (pointer >> 2));
}

void RhiRenderer::setGpuCapacity(uint64_t bytes) {
	gpuCapacity = bytes;
	geometry.clear();
	textures.clear();
	uploadData.clear();
	textureBytes = 0;
	gpuMemory.reset();
	geometryBuffersDirty = true;
}

bool RhiRenderer::initialize(QRhi *newRhi, QRhiCommandBuffer *cb, QRhiRenderTarget *target) {
	QRhiRenderPassDescriptor *newRenderPassDescriptor = target->renderPassDescriptor();
	if(initialized && rhi == newRhi && renderTarget == target &&
	   renderPassDescriptor == newRenderPassDescriptor)
		return true;

	releaseResources();
	rhi = newRhi;
	renderPassDescriptor = newRenderPassDescriptor;
	renderTarget = target;
	initialized = createResources(cb, target);
	return initialized;
}

void RhiRenderer::releaseResources() {
	if(resourceUpdates) {
		resourceUpdates->release();
		resourceUpdates = nullptr;
	}
	frameActive = false;
	draws.clear();
	frameBindings.clear();
	geometry.clear();
	textures.clear();
	textureBytes = 0;
	trianglePipeline.reset();
	pointPipeline.reset();
	pipelineBindings.reset();
	sampler.reset();
	whiteTexture.reset();
	uniformBuffer.reset();
	vertexBuffer.reset();
	indexBuffer.reset();
	gpuMemory.reset();
	vertexShader = {};
	fragmentShader = {};
	rhi = nullptr;
	renderPassDescriptor = nullptr;
	commandBuffer = nullptr;
	renderTarget = nullptr;
	initialized = false;
	geometryBuffersDirty = true;
}

bool RhiRenderer::createResources(QRhiCommandBuffer *cb, QRhiRenderTarget *target) {
	initializeRhiShaderResources();
	if(!createGeometryBuffers())
		return false;

	uniformStride = (sizeof(UniformData) + static_cast<std::size_t>(rhi->ubufAlignment()) - 1) /
	                static_cast<std::size_t>(rhi->ubufAlignment()) *
	                static_cast<std::size_t>(rhi->ubufAlignment());
	uniformCapacity = uniformStride * 256;
	uniformBuffer.reset(rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer,
	                                   static_cast<int>(uniformCapacity)));
	if(!uniformBuffer->create()) {
		qWarning() << "Failed to create RHI uniform buffer";
		return false;
	}

	sampler.reset(rhi->newSampler(QRhiSampler::Linear, QRhiSampler::Linear,
	                              QRhiSampler::None, QRhiSampler::Repeat,
	                              QRhiSampler::Repeat));
	if(!sampler->create()) {
		qWarning() << "Failed to create RHI texture sampler";
		return false;
	}

	whiteTexture.reset(rhi->newTexture(QRhiTexture::RGBA8, QSize(1, 1)));
	if(!whiteTexture->create()) {
		qWarning() << "Failed to create default RHI texture";
		return false;
	}
	QByteArray whitePixel(4, char(0xff));
	auto *initialUpdates = rhi->nextResourceUpdateBatch();
	QRhiTextureUploadDescription whiteUpload;
	whiteUpload.setEntries({QRhiTextureUploadEntry(
		0, 0, QRhiTextureSubresourceUploadDescription(whitePixel))});
	initialUpdates->uploadTexture(whiteTexture.get(), whiteUpload);
	cb->resourceUpdate(initialUpdates);

	vertexShader = readShader(QStringLiteral(":/nxsview/qrhi/shaders/mesh.vert.qsb"));
	fragmentShader = readShader(QStringLiteral(":/nxsview/qrhi/shaders/mesh.frag.qsb"));
	if(!vertexShader.isValid() || !fragmentShader.isValid())
		return false;

	return createBindingsAndPipelines() && target->renderPassDescriptor();
}

bool RhiRenderer::createGeometryBuffers() {
	constexpr std::size_t slotCount = 16;
	const uint64_t geometryCapacity = std::min<uint64_t>(
		gpuCapacity * 3 / 4, 1024ull * 1024 * 1024);
	const std::size_t bytesPerSlot = static_cast<std::size_t>(geometryCapacity / slotCount);
	const std::size_t maxVertexBytes = bytesPerSlot * 3 / 4;
	const std::size_t maxIndexBytes = bytesPerSlot - maxVertexBytes;
	if(maxVertexBytes == 0 || maxIndexBytes == 0) {
		qWarning() << "RHI GPU cache capacity is too small for geometry slots";
		return false;
	}

	rhi::GpuMemory::GeometryConfig config;
	config.slotCount = slotCount;
	config.maxVertexBytesPerSlot = maxVertexBytes;
	config.maxIndexBytesPerSlot = maxIndexBytes;
	config.vertexAlignment = 256;
	config.indexAlignment = 4;
	gpuMemory = std::make_unique<rhi::GpuMemory>(config, 256);

	const std::size_t vertexSize = gpuMemory->vertexBufferSize();
	const std::size_t indexSize = gpuMemory->indexBufferSize();
	if(vertexSize > static_cast<std::size_t>(std::numeric_limits<int>::max()) ||
	   indexSize > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
		qWarning() << "RHI geometry cache exceeds the QRhi buffer size limit";
		return false;
	}

	vertexBuffer.reset(rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer,
	                                  static_cast<int>(vertexSize)));
	indexBuffer.reset(rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::IndexBuffer,
	                                 static_cast<int>(indexSize)));
	if(!vertexBuffer->create() || !indexBuffer->create()) {
		qWarning() << "Failed to create RHI geometry cache buffers";
		vertexBuffer.reset();
		indexBuffer.reset();
		return false;
	}

	textureCapacity = static_cast<std::size_t>(
		std::min<uint64_t>(gpuCapacity / 4, 1024ull * 1024 * 1024));
	geometryBuffersDirty = false;
	return true;
}

bool RhiRenderer::createBindingsAndPipelines() {
	trianglePipeline.reset();
	pointPipeline.reset();
	pipelineBindings.reset(rhi->newShaderResourceBindings());
	pipelineBindings->setBindings({
		QRhiShaderResourceBinding::uniformBuffer(
			0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
			uniformBuffer.get(), 0, sizeof(UniformData)),
		QRhiShaderResourceBinding::sampledTexture(
			1, QRhiShaderResourceBinding::FragmentStage, whiteTexture.get(), sampler.get())
	});
	if(!pipelineBindings->create()) {
		qWarning() << "Failed to create RHI shader resource bindings";
		return false;
	}

	QRhiVertexInputLayout inputLayout;
	inputLayout.setBindings({QRhiVertexInputBinding(sizeof(Vertex))});
	inputLayout.setAttributes({
		QRhiVertexInputAttribute(0, 0, QRhiVertexInputAttribute::Float3,
		                         offsetof(Vertex, position)),
		QRhiVertexInputAttribute(0, 1, QRhiVertexInputAttribute::Float3,
		                         offsetof(Vertex, normal)),
		QRhiVertexInputAttribute(0, 2, QRhiVertexInputAttribute::Float4,
		                         offsetof(Vertex, color)),
		QRhiVertexInputAttribute(0, 3, QRhiVertexInputAttribute::Float2,
		                         offsetof(Vertex, texCoord))
	});

	auto createPipeline = [&](QRhiGraphicsPipeline::Topology topology) {
		auto pipeline = std::unique_ptr<QRhiGraphicsPipeline>(rhi->newGraphicsPipeline());
		pipeline->setShaderStages({
			{QRhiShaderStage::Vertex, vertexShader},
			{QRhiShaderStage::Fragment, fragmentShader}
		});
		pipeline->setVertexInputLayout(inputLayout);
		pipeline->setShaderResourceBindings(pipelineBindings.get());
		pipeline->setRenderPassDescriptor(renderPassDescriptor);
		pipeline->setSampleCount(renderTarget->sampleCount());
		pipeline->setTopology(topology);
		pipeline->setDepthTest(true);
		pipeline->setDepthWrite(true);
		if(!pipeline->create()) {
			qWarning() << "Failed to create RHI graphics pipeline with topology" << static_cast<int>(topology);
			checkRhiErrors("QRhiGraphicsPipeline::create");
			return std::unique_ptr<QRhiGraphicsPipeline>();
		}
		return pipeline;
	};

	trianglePipeline = createPipeline(QRhiGraphicsPipeline::Triangles);
	pointPipeline = createPipeline(QRhiGraphicsPipeline::Points);
	return trianglePipeline && pointPipeline;
}

bool RhiRenderer::ensureUniformCapacity(std::size_t drawCount) {
	const std::size_t required = drawCount * uniformStride;
	if(required <= uniformCapacity)
		return true;

	std::size_t newCapacity = uniformCapacity;
	while(newCapacity < required)
		newCapacity *= 2;
	if(newCapacity > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
		qWarning() << "RHI draw uniform data exceeds the QRhi buffer size limit";
		return false;
	}

	trianglePipeline.reset();
	pointPipeline.reset();
	pipelineBindings.reset();
	uniformBuffer.reset(rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer,
	                                   static_cast<int>(newCapacity)));
	if(!uniformBuffer->create()) {
		qWarning() << "Failed to grow RHI uniform buffer";
		return false;
	}
	uniformCapacity = newCapacity;
	return createBindingsAndPipelines();
}

bool RhiRenderer::beginRhiFrame(QRhiCommandBuffer *cb, QRhiRenderTarget *target, QSize viewport) {
	assert(initialized && !frameActive);
	if(geometryBuffersDirty && !createGeometryBuffers())
		return false;
	commandBuffer = cb;
	renderTarget = target;
	viewportSize = viewport;
	draws.clear();
	frameBindings.clear();
	uploadData.clear();
#ifdef NXSVIEW_DEBUG_NODE_LOADING
	pendingGeometryUploads.clear();
#endif
	warnedCacheOverflow = false;
	Renderer::startFrame();
	resourceUpdates = rhi->nextResourceUpdateBatch();
	if(!resourceUpdates) {
		qWarning() << "Failed to allocate QRhi resource update batch";
		return false;
	}
	frameActive = true;
	checkRhiErrors("beginRhiFrame");
	return true;
}

void RhiRenderer::setViewMatrices(const QMatrix4x4 &projection,
                                  const QMatrix4x4 &modelView,
                                  const QMatrix4x4 &model,
                                  const QMatrix4x4 &clipSpaceCorrection,
                                  const int *viewport) {
	projectionMatrix = projection;
	modelViewMatrix = modelView;
	modelMatrix = model;
	mvpMatrix = clipSpaceCorrection * projection * modelView;
	std::copy(viewport, viewport + 4, currentViewport);
	Renderer::getView(projectionMatrix.constData(), modelViewMatrix.constData(), currentViewport);
}

void RhiRenderer::getView() {
	Renderer::getView(projectionMatrix.constData(), modelViewMatrix.constData(), currentViewport);
}

void RhiRenderer::beginRender(const RenderOptions &) {}

void RhiRenderer::endRender(const RenderOptions &) {}

bool RhiRenderer::evictGeometry() {
	auto candidate = geometry.end();
	for(auto it = geometry.begin(); it != geometry.end(); ++it) {
		if(it->second.lastUsedFrame == frame)
			continue;
		if(candidate == geometry.end() ||
		   it->second.lastUsedFrame < candidate->second.lastUsedFrame)
			candidate = it;
	}
	if(candidate == geometry.end())
		return false;

	if(!candidate->second.dedicated)
		gpuMemory->releaseGeometry(candidate->second.allocation.slot);
	geometry.erase(candidate);
	return true;
}

bool RhiRenderer::ensureGeometry(Nexus *nexus, uint32_t nodeIndex, GeometryEntry *&entry) {
	const GeometryKey key{nexus, nodeIndex};
	auto found = geometry.find(key);
	if(found != geometry.end()) {
		found->second.lastUsedFrame = frame;
		entry = &found->second;
		return true;
	}

	Node &node = nexus->nodes[nodeIndex];
	NodeData &nodeData = nexus->nodedata[nodeIndex];
	assert(nodeData.memory);
	Signature &signature = nexus->header.signature;
	const VertexElement &vertexFormat = signature.vertex;
	const Attribute &coordinate = vertexFormat.attributes[VertexElement::COORD];
	if(!hasReadableAttribute(coordinate, 3)) {
		qWarning() << "RHI vertex format has no 3-component coordinate attribute" << nodeIndex;
		return false;
	}
	if((vertexFormat.hasNormals() &&
	    !hasReadableAttribute(vertexFormat.attributes[VertexElement::NORM], 3)) ||
	   (vertexFormat.hasColors() &&
	    !hasReadableAttribute(vertexFormat.attributes[VertexElement::COLOR], 3)) ||
	   (vertexFormat.hasTextures() &&
	    !hasReadableAttribute(vertexFormat.attributes[VertexElement::TEX], 2))) {
		qWarning() << "RHI vertex format has an attribute with too few components" << nodeIndex;
		return false;
	}

	const std::size_t vertexBytes = static_cast<std::size_t>(node.nvert) * sizeof(Vertex);
	const uint64_t indexCount64 = signature.face.hasIndex()
		? static_cast<uint64_t>(node.nface) * 3 : 0;
	if(indexCount64 > std::numeric_limits<uint32_t>::max()) {
		qWarning() << "RHI node index count exceeds the supported draw size" << nodeIndex;
		return false;
	}
	const uint32_t indexCount = static_cast<uint32_t>(indexCount64);
	const std::size_t indexBytes = static_cast<std::size_t>(indexCount) * sizeof(uint16_t);
	if(vertexBytes > static_cast<std::size_t>(std::numeric_limits<int>::max()) ||
	   indexBytes > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
		qWarning() << "RHI node buffer exceeds the QRhi buffer size limit" << nodeIndex;
		return false;
	}
	auto allocation = gpuMemory->allocateGeometry(vertexBytes, indexBytes);
	while(!allocation && evictGeometry())
		allocation = gpuMemory->allocateGeometry(vertexBytes, indexBytes);

	std::vector<Vertex> vertices(node.nvert);
	const std::size_t sourceStride = vertexFormat.vertexSize();

	for(uint32_t i = 0; i < node.nvert; ++i) {
		Vertex &vertex = vertices[i];
		const char *sourceVertex = nodeData.memory + static_cast<std::size_t>(i) * sourceStride;
		for(unsigned int c = 0; c < 3; ++c)
			readVertexValue(sourceVertex, vertexFormat, VertexElement::COORD, c, false, vertex.position[c]);

		if(vertexFormat.hasNormals()) {
			for(unsigned int c = 0; c < 3; ++c)
				readVertexValue(sourceVertex, vertexFormat, VertexElement::NORM, c, true, vertex.normal[c]);
		} else {
			vertex.normal[0] = 0;
			vertex.normal[1] = 0;
			vertex.normal[2] = 1;
		}
		if(vertexFormat.hasColors()) {
			for(unsigned int c = 0; c < 3; ++c)
				readVertexValue(sourceVertex, vertexFormat, VertexElement::COLOR, c, true, vertex.color[c]);
			vertex.color[3] = 1.0f;
			readVertexValue(sourceVertex, vertexFormat, VertexElement::COLOR, 3, true, vertex.color[3]);
		} else {
			vertex.color[0] = vertex.color[1] = vertex.color[2] = vertex.color[3] = 1;
		}
		if(vertexFormat.hasTextures()) {
			for(unsigned int c = 0; c < 2; ++c)
				readVertexValue(sourceVertex, vertexFormat, VertexElement::TEX, c, true, vertex.texCoord[c]);
		} else {
			vertex.texCoord[0] = vertex.texCoord[1] = 0;
		}
	}

	GeometryEntry newEntry{};
	newEntry.vertexCount = node.nvert;
	newEntry.indexCount = indexCount;
	newEntry.lastUsedFrame = frame;
	if(allocation) {
		newEntry.allocation = *allocation;
	} else {
		if(!warnedCacheOverflow) {
			qWarning() << "RHI geometry cache is full or a node exceeds a cache slot;"
			              " using temporary buffers for this frame";
			warnedCacheOverflow = true;
		}
		newEntry.dedicated = true;
		newEntry.allocation = {
			std::numeric_limits<rhi::GpuMemory::GeometrySlot>::max(),
			0, 0, vertexBytes, indexBytes
		};
		newEntry.dedicatedVertexBuffer.reset(rhi->newBuffer(
			QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer, static_cast<int>(vertexBytes)));
		if(!newEntry.dedicatedVertexBuffer->create()) {
			qWarning() << "Failed to create temporary RHI vertex buffer for node" << nodeIndex;
			return false;
		}
		if(indexBytes) {
			newEntry.dedicatedIndexBuffer.reset(rhi->newBuffer(
				QRhiBuffer::Dynamic, QRhiBuffer::IndexBuffer, static_cast<int>(indexBytes)));
			if(!newEntry.dedicatedIndexBuffer->create()) {
				qWarning() << "Failed to create temporary RHI index buffer for node" << nodeIndex;
				return false;
			}
		}
	}

	QRhiBuffer *vertexDestination = newEntry.dedicated
		? newEntry.dedicatedVertexBuffer.get() : vertexBuffer.get();
	QRhiBuffer *indexDestination = newEntry.dedicated
		? newEntry.dedicatedIndexBuffer.get() : indexBuffer.get();
	const quint32 vertexOffset = newEntry.dedicated
		? 0 : static_cast<quint32>(allocation->vertexOffset);
	const quint32 indexOffset = newEntry.dedicated
		? 0 : static_cast<quint32>(allocation->indexOffset);
	uploadData.emplace_back(reinterpret_cast<const char *>(vertices.data()),
	                        static_cast<qsizetype>(vertexBytes));
	resourceUpdates->updateDynamicBuffer(
		vertexDestination, vertexOffset,
		static_cast<quint32>(vertexBytes), uploadData.back().constData());
	if(indexBytes) {
		uint16_t *indices = nodeData.faces(signature, node.nvert);
		uploadData.emplace_back(reinterpret_cast<const char *>(indices),
		                        static_cast<qsizetype>(indexBytes));
		resourceUpdates->updateDynamicBuffer(
			indexDestination, indexOffset,
			static_cast<quint32>(indexBytes), uploadData.back().constData());
	}

	auto inserted = geometry.emplace(key, std::move(newEntry));
	entry = &inserted.first->second;
#ifdef NXSVIEW_DEBUG_NODE_LOADING
	pendingGeometryUploads.emplace_back(nexus, nodeIndex);
	qInfo() << "QRhi geometry upload queued" << nodeIndex << "from"
	        << QString::fromStdString(nexus->url)
	        << "vertex bytes:" << vertexBytes << "index bytes:" << indexBytes;
#endif
	return true;
}

bool RhiRenderer::evictTexture() {
	auto candidate = textures.end();
	for(auto it = textures.begin(); it != textures.end(); ++it) {
		if(it->second.lastUsedFrame == frame)
			continue;
		if(candidate == textures.end() ||
		   it->second.lastUsedFrame < candidate->second.lastUsedFrame)
			candidate = it;
	}
	if(candidate == textures.end())
		return false;

	textureBytes -= candidate->second.bytes;
	textures.erase(candidate);
	return true;
}

QRhiTexture *RhiRenderer::textureFor(Nexus *nexus, uint32_t textureIndex) {
	const TextureKey key{nexus, textureIndex};
	auto found = textures.find(key);
	if(found != textures.end()) {
		found->second.lastUsedFrame = frame;
		return found->second.texture.get();
	}

	TextureData &data = nexus->texturedata[textureIndex];
	std::lock_guard<std::mutex> lock(data.mutex);
	if(!data.memory || !data.width || !data.height)
		return whiteTexture.get();

	const std::size_t bytes = static_cast<std::size_t>(data.width) * data.height * 4;
	if(bytes > static_cast<std::size_t>(std::numeric_limits<int>::max()) ||
	   bytes > textureCapacity) {
		qWarning() << "RHI texture exceeds the configured texture cache capacity"
		           << textureIndex << "from" << QString::fromStdString(nexus->url);
		return whiteTexture.get();
	}
	while(bytes > textureCapacity - textureBytes && evictTexture()) {}
	if(bytes > textureCapacity - textureBytes) {
		qWarning() << "RHI texture cache is full; texture" << textureIndex
		           << "from" << QString::fromStdString(nexus->url) << "will not be shown";
		return whiteTexture.get();
	}

	auto texture = std::unique_ptr<QRhiTexture>(
		rhi->newTexture(QRhiTexture::RGBA8, QSize(data.width, data.height)));
	if(!texture->create()) {
		qWarning() << "Failed to create RHI texture" << textureIndex;
		checkRhiErrors("QRhiTexture::create");
		return whiteTexture.get();
	}

	QByteArray pixels(data.memory, static_cast<int>(bytes));
	QRhiTextureUploadDescription upload;
	upload.setEntries({QRhiTextureUploadEntry(
		0, 0, QRhiTextureSubresourceUploadDescription(pixels))});
	resourceUpdates->uploadTexture(texture.get(), upload);
	checkRhiErrors("uploadTexture");

	QRhiTexture *result = texture.get();
	textures.emplace(key, TextureEntry{std::move(texture), bytes, frame});
	textureBytes += bytes;
	return result;
}

void RhiRenderer::appendDraw(Nexus *nexus, uint32_t nodeIndex, const GeometryEntry &entry,
                             uint32_t firstIndex, uint32_t indexCount, bool points,
                             const RenderOptions &options, uint32_t textureIndex,
                             const QColor &materialColor) {
	const bool useTexture = options.draw_textures && textureIndex != 0xffffffff;
	QRhiTexture *texture = useTexture ? textureFor(nexus, textureIndex) : whiteTexture.get();
	const bool textureReady = useTexture && texture != whiteTexture.get();
	QMatrix4x4 texMatrix;
	if(textureReady && !options.draw_texcoords)
		texMatrix = textureMatrix(nexus->textures[textureIndex].matrix);

	const QColor color = options.draw_colors ? QColor(Qt::white) : materialColor;
	draws.push_back(DrawCall{
		entry.allocation,
		entry.vertexCount,
		indexCount,
		firstIndex,
		!points && entry.indexCount != 0,
		points,
		textureReady,
		options.draw_colors,
		options.draw_normals,
		textureReady && !options.draw_texcoords,
		mvpMatrix,
		modelMatrix,
		texMatrix,
		color,
		texture,
		entry.dedicated ? entry.dedicatedVertexBuffer.get() : vertexBuffer.get(),
		entry.dedicated ? entry.dedicatedIndexBuffer.get() : indexBuffer.get()
	});
}

void RhiRenderer::renderSelected(Nexus *nexus, const RenderOptions &options) {
	for(uint32_t i = 0; i <= last_node; ++i) {
		if(!selected[i])
			continue;

		Node &node = nexus->nodes[i];
		if(nexus->header.signature.face.hasIndex() && skipNode(i))
			continue;

		++stats.node_rendered;
		const Sphere sphere = node.tightSphere();
		if(frustum_culling && metric.frustum.isOutside(sphere.Center(), sphere.Radius())) {
			++stats.frustum_culled;
			continue;
		}
		if(cone_culling && node.cone.Backface(sphere, metric.frustum.viewPoint())) {
			++stats.cone_culled;
			continue;
		}

		GeometryEntry *entry = nullptr;
		if(!ensureGeometry(nexus, i, entry))
			continue;

		if(!options.draw_triangles) {
			appendDraw(nexus, i, *entry, 0, 0, true, options, 0xffffffff,
			           toQColor(meshColor));
			++stats.patch_rendered;
			stats.instance_rendered += node.nvert;
			continue;
		}

		uint32_t offset = 0;
		uint32_t end = 0;
		const uint32_t lastPatch = node.last_patch() - 1;
		for(uint32_t patchIndex = node.first_patch; patchIndex < node.last_patch(); ++patchIndex) {
			Patch &patch = nexus->patches[patchIndex];
			if(!selected[patch.node]) {
				end = patch.triangle_offset;
				if(patchIndex != lastPatch)
					continue;
			}
			if(end > offset) {
				QColor patchColor = toQColor(meshColor);
				if(mode & PATCHES) {
					patchColor = QColor(((i % 16) * 135) % 127 + 127,
					                    ((i % 7) * 57) % 127 + 127,
					                    ((i % 11) * 171) % 63 + 63);
				}
				appendDraw(nexus, i, *entry, offset * 3, (end - offset) * 3,
				           false, options, patch.texture, patchColor);
				++stats.patch_rendered;
				stats.instance_rendered += end - offset;
			}
			offset = patch.triangle_offset;
		}
	}
}

void RhiRenderer::finishRhiFrame(const QColor &clearColor) {
	assert(frameActive);
	if(!ensureUniformCapacity(draws.size())) {
		draws.clear();
	}

	uniformData.resize(draws.size() * uniformStride);
	frameBindings.clear();
	frameBindings.reserve(draws.size());
	const QRhiShaderResourceBinding::StageFlags stages =
		QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage;
	for(std::size_t i = 0; i < draws.size(); ++i) {
		const DrawCall &draw = draws[i];
		UniformData uniforms{};
		std::copy(draw.mvp.constData(), draw.mvp.constData() + 16, uniforms.mvp);
		std::copy(draw.model.constData(), draw.model.constData() + 16, uniforms.model);
		std::copy(draw.textureMatrix.constData(), draw.textureMatrix.constData() + 16,
		          uniforms.textureMatrix);
		uniforms.color[0] = draw.color.redF();
		uniforms.color[1] = draw.color.greenF();
		uniforms.color[2] = draw.color.blueF();
		uniforms.color[3] = draw.color.alphaF();
		uniforms.options[0] = draw.useTexture ? 1.0f : 0.0f;
		uniforms.options[1] = draw.useVertexColors ? 1.0f : 0.0f;
		uniforms.options[2] = draw.useLighting ? 1.0f : 0.0f;
		uniforms.options[3] = draw.generatedTexCoords ? 1.0f : 0.0f;

		const std::size_t offset = i * uniformStride;
		std::memcpy(uniformData.data() + offset, &uniforms, sizeof(uniforms));
		resourceUpdates->updateDynamicBuffer(
			uniformBuffer.get(), static_cast<quint32>(offset),
			static_cast<quint32>(sizeof(uniforms)), uniformData.data() + offset);

		auto bindings = std::unique_ptr<QRhiShaderResourceBindings>(
			rhi->newShaderResourceBindings());
		bindings->setBindings({
			QRhiShaderResourceBinding::uniformBuffer(
				0, stages, uniformBuffer.get(), static_cast<quint32>(offset), sizeof(UniformData)),
			QRhiShaderResourceBinding::sampledTexture(
				1, QRhiShaderResourceBinding::FragmentStage, draw.texture, sampler.get())
		});
		if(!bindings->create()) {
			qWarning() << "Failed to create RHI draw bindings";
			frameBindings.push_back(nullptr);
		} else {
			frameBindings.push_back(std::move(bindings));
		}
	}

	if(resourceUpdates) {
		commandBuffer->resourceUpdate(resourceUpdates);
		resourceUpdates = nullptr;
		checkRhiErrors("resourceUpdate");
#ifdef NXSVIEW_DEBUG_NODE_LOADING
		for(const auto &[nexus, nodeIndex] : pendingGeometryUploads) {
			qInfo() << "QRhi geometry upload submitted" << nodeIndex << "from"
			        << QString::fromStdString(nexus->url);
		}
		pendingGeometryUploads.clear();
#endif
	}

	commandBuffer->beginPass(renderTarget, clearColor, {1.0f, 0});
	checkRhiErrors("beginPass");
	if(rhi->isDeviceLost()) {
		qWarning() << "Device lost after beginning render pass";
		draws.clear();
		frameActive = false;
		return;
	}

	commandBuffer->setViewport(QRhiViewport(0, 0, viewportSize.width(), viewportSize.height()));
	for(std::size_t i = 0; i < draws.size(); ++i) {
		if(!frameBindings[i])
			continue;
		const DrawCall &draw = draws[i];

		if(!draw.points && !trianglePipeline) {
			qWarning() << "Triangle pipeline not available for draw" << i;
			continue;
		}
		if(draw.points && !pointPipeline) {
			qWarning() << "Point pipeline not available for draw" << i;
			continue;
		}

		commandBuffer->setGraphicsPipeline(draw.points ? pointPipeline.get() : trianglePipeline.get());
		commandBuffer->setShaderResources(frameBindings[i].get());
		const QRhiCommandBuffer::VertexInput vertexInput(
			draw.vertexBuffer, static_cast<quint32>(draw.allocation.vertexOffset));
		if(draw.indexed) {
			if(!draw.indexBuffer) {
				qWarning() << "Index buffer is null for indexed draw" << i;
				continue;
			}
			commandBuffer->setVertexInput(0, 1, &vertexInput, draw.indexBuffer,
			                              static_cast<quint32>(draw.allocation.indexOffset),
			                              QRhiCommandBuffer::IndexUInt16);
			commandBuffer->drawIndexed(draw.indexCount, 1, draw.firstIndex, 0, 0);
		} else {
			commandBuffer->setVertexInput(0, 1, &vertexInput);
			commandBuffer->draw(draw.vertexCount, 1, 0, 0);
		}
	}

	commandBuffer->endPass();
	checkRhiErrors("endPass");

	for(auto it = geometry.begin(); it != geometry.end();) {
		if(it->second.dedicated && it->second.lastUsedFrame == frame)
			it = geometry.erase(it);
		else
			++it;
	}
	draws.clear();
	frameActive = false;
}
