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
#ifndef NX_RHI_RENDERER_H
#define NX_RHI_RENDERER_H

#include <memory>
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include <QByteArray>
#include <QColor>
#include <QMatrix4x4>
#include <QSize>
#include <rhi/qshader.h>
#include <rhi/qrhi.h>

#include "../../renderer.h"
#include "gpu_cache_allocator.h"

namespace nx {

class RhiRenderer : public Renderer {
public:
	RhiRenderer();
	~RhiRenderer() override;

	bool initialize(QRhi *rhi, QRhiCommandBuffer *cb, QRhiRenderTarget *target);
	void releaseResources();
	bool isReady() const { return initialized; }

	bool beginRhiFrame(QRhiCommandBuffer *cb, QRhiRenderTarget *target, QSize viewport);
	void finishRhiFrame(const QColor &clearColor);
	void setViewMatrices(const QMatrix4x4 &projection,
	                     const QMatrix4x4 &modelView,
	                     const QMatrix4x4 &model,
	                     const QMatrix4x4 &clipSpaceCorrection,
	                     const int *viewport);
	void setMeshColor(Rgba8 color) { color.a = 255; meshColor = color; }
	void setGpuCapacity(uint64_t bytes) override;
	void getView() override;

protected:
	void beginRender(const RenderOptions &options) override;
	void renderSelected(Nexus *nexus, const RenderOptions &options) override;
	void endRender(const RenderOptions &options) override;

private:
	struct GeometryKey {
		Nexus *nexus;
		uint32_t node;

		bool operator==(const GeometryKey &other) const noexcept {
			return nexus == other.nexus && node == other.node;
		}
	};

	struct GeometryKeyHash {
		std::size_t operator()(const GeometryKey &key) const noexcept;
	};

	struct GeometryEntry {
		rhi::GpuMemory::GeometryAllocation allocation;
		uint32_t vertexCount;
		uint32_t indexCount;
		uint64_t lastUsedFrame;
		bool dedicated = false;
		std::unique_ptr<QRhiBuffer> dedicatedVertexBuffer;
		std::unique_ptr<QRhiBuffer> dedicatedIndexBuffer;
	};

	struct TextureKey {
		Nexus *nexus;
		uint32_t texture;

		bool operator==(const TextureKey &other) const noexcept {
			return nexus == other.nexus && texture == other.texture;
		}
	};

	struct TextureKeyHash {
		std::size_t operator()(const TextureKey &key) const noexcept;
	};

	struct TextureEntry {
		std::unique_ptr<QRhiTexture> texture;
		std::size_t bytes;
		uint64_t lastUsedFrame;
	};

	struct DrawCall {
		rhi::GpuMemory::GeometryAllocation allocation;
		uint32_t vertexCount;
		uint32_t indexCount;
		uint32_t firstIndex;
		bool indexed;
		bool points;
		bool useTexture;
		bool useVertexColors;
		bool useLighting;
		bool generatedTexCoords;
		QMatrix4x4 mvp;
		QMatrix4x4 model;
		QMatrix4x4 textureMatrix;
		QColor color;
		QRhiTexture *texture;
		QRhiBuffer *vertexBuffer;
		QRhiBuffer *indexBuffer;
	};

	struct alignas(16) UniformData {
		float mvp[16];
		float model[16];
		float textureMatrix[16];
		float color[4];
		float options[4];
	};

	struct Vertex {
		float position[3];
		float normal[3];
		float color[4];
		float texCoord[2];
	};

	void checkRhiErrors(const char *context);
	bool createResources(QRhiCommandBuffer *cb, QRhiRenderTarget *target);
	bool createGeometryBuffers();
	bool createBindingsAndPipelines();
	bool ensureUniformCapacity(std::size_t drawCount);
	bool ensureGeometry(Nexus *nexus, uint32_t node, GeometryEntry *&entry);
	QRhiTexture *textureFor(Nexus *nexus, uint32_t texture);
	bool evictGeometry();
	bool evictTexture();
	void appendDraw(Nexus *nexus, uint32_t node, const GeometryEntry &geometry,
	                uint32_t firstIndex, uint32_t indexCount, bool points,
	                const RenderOptions &options, uint32_t texture,
	                const QColor &color);

	QRhi *rhi = nullptr;
	QRhiRenderPassDescriptor *renderPassDescriptor = nullptr;
	QRhiCommandBuffer *commandBuffer = nullptr;
	QRhiRenderTarget *renderTarget = nullptr;
	QSize viewportSize;
	QMatrix4x4 projectionMatrix;
	QMatrix4x4 modelViewMatrix;
	QMatrix4x4 modelMatrix;
	QMatrix4x4 mvpMatrix;
	int currentViewport[4]{};
	Rgba8 meshColor{255, 255, 255, 255};
	bool initialized = false;
	bool frameActive = false;
	bool geometryBuffersDirty = true;
	bool warnedCacheOverflow = false;

	uint64_t gpuCapacity = 256ull * 1024 * 1024;
	std::size_t textureCapacity = 0;
	std::size_t textureBytes = 0;
	std::size_t uniformStride = 0;
	std::size_t uniformCapacity = 0;

	std::unique_ptr<rhi::GpuMemory> gpuMemory;
	std::unique_ptr<QRhiBuffer> vertexBuffer;
	std::unique_ptr<QRhiBuffer> indexBuffer;
	std::unique_ptr<QRhiBuffer> uniformBuffer;
	std::unique_ptr<QRhiTexture> whiteTexture;
	std::unique_ptr<QRhiSampler> sampler;
	std::unique_ptr<QRhiShaderResourceBindings> pipelineBindings;
	std::unique_ptr<QRhiGraphicsPipeline> trianglePipeline;
	std::unique_ptr<QRhiGraphicsPipeline> pointPipeline;
	QShader vertexShader;
	QShader fragmentShader;
	QRhiResourceUpdateBatch *resourceUpdates = nullptr;

	std::unordered_map<GeometryKey, GeometryEntry, GeometryKeyHash> geometry;
	std::unordered_map<TextureKey, TextureEntry, TextureKeyHash> textures;
#if defined(NXSVIEW_DEBUG_NODE_LOADING) && NXSVIEW_DEBUG_NODE_LOADING
	std::vector<std::pair<Nexus *, uint32_t>> pendingGeometryUploads;
#endif
	std::vector<DrawCall> draws;
	std::vector<std::unique_ptr<QRhiShaderResourceBindings>> frameBindings;
	std::vector<std::byte> uniformData;
	std::vector<QByteArray> uploadData;
};

} // namespace nx

#endif // NX_RHI_RENDERER_H
