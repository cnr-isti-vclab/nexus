#include "gpu_cache_allocator.h"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace nx::rhi {
namespace {

constexpr std::size_t BitsPerWord = 64;

std::size_t bitmapWordCount(std::size_t bitCount) {
    return bitCount / BitsPerWord + (bitCount % BitsPerWord != 0 ? 1 : 0);
}

bool bitIsSet(const std::vector<std::uint64_t> &bits, std::size_t index) {
    return (bits[index / BitsPerWord] & (std::uint64_t{1} << (index % BitsPerWord))) != 0;
}

void setBit(std::vector<std::uint64_t> &bits, std::size_t index) {
    bits[index / BitsPerWord] |= std::uint64_t{1} << (index % BitsPerWord);
}

void clearBit(std::vector<std::uint64_t> &bits, std::size_t index) {
    bits[index / BitsPerWord] &= ~(std::uint64_t{1} << (index % BitsPerWord));
}

} // namespace

GpuMemory::GpuMemory(GeometryConfig geometry,
                                     std::uint32_t maxLayersPerTextureArray)
    : geometry(geometry),
      vertexStrideBytes(0),
      indexStrideBytes(0),
      layerCapacity(maxLayersPerTextureArray) {
    if(maxLayersPerTextureArray == 0)
        throw std::invalid_argument("texture-array layer capacity must be nonzero");
    if(geometry.slotCount > std::numeric_limits<GeometrySlot>::max())
        throw std::invalid_argument("geometry slot count exceeds the slot identifier range");
    if(geometry.vertexAlignment == 0 || geometry.indexAlignment == 0)
        throw std::invalid_argument("geometry buffer alignments must be nonzero");
    if(geometry.slotCount != 0 &&
       (geometry.maxVertexBytesPerSlot == 0 || geometry.maxIndexBytesPerSlot == 0))
        throw std::invalid_argument("enabled geometry slots require nonzero vertex and index capacities");

    if(geometry.slotCount != 0) {
        vertexStrideBytes = alignedSize(geometry.maxVertexBytesPerSlot,
                                        geometry.vertexAlignment);
        indexStrideBytes = alignedSize(geometry.maxIndexBytesPerSlot,
                                       geometry.indexAlignment);
        if(vertexStrideBytes > std::numeric_limits<std::size_t>::max() / geometry.slotCount ||
           indexStrideBytes > std::numeric_limits<std::size_t>::max() / geometry.slotCount)
            throw std::overflow_error("geometry arena size overflows size_t");
        allocatedGeometrySlots.resize(bitmapWordCount(geometry.slotCount));
    }
}

std::size_t GpuMemory::alignedSize(std::size_t size, std::size_t alignment) {
    const std::size_t remainder = size % alignment;
    if(remainder == 0)
        return size;
    const std::size_t padding = alignment - remainder;
    if(size > std::numeric_limits<std::size_t>::max() - padding)
        throw std::overflow_error("aligned geometry slot size overflows size_t");
    return size + padding;
}

bool GpuMemory::isValidLayout(const TextureArrayLayout &layout) noexcept {
    return layout.width != 0 && layout.height != 0 && layout.mipLevels != 0 &&
           layout.sampleCount != 0;
}

std::optional<GpuMemory::GeometryAllocation>
GpuMemory::allocateGeometry(std::size_t vertexBytes, std::size_t indexBytes) {
    if(geometry.slotCount == 0 || (vertexBytes == 0 && indexBytes == 0) ||
       vertexBytes > geometry.maxVertexBytesPerSlot ||
       indexBytes > geometry.maxIndexBytesPerSlot)
        return std::nullopt;

    for(std::size_t slot = geometryCursor; slot < geometry.slotCount; ++slot) {
        if(bitIsSet(allocatedGeometrySlots, slot))
            continue;

        setBit(allocatedGeometrySlots, slot);
        ++allocatedGeometryCount;
        geometryCursor = slot + 1;
        while(geometryCursor < geometry.slotCount &&
              bitIsSet(allocatedGeometrySlots, geometryCursor))
            ++geometryCursor;

        return GeometryAllocation{
            static_cast<GeometrySlot>(slot),
            slot * vertexStrideBytes,
            slot * indexStrideBytes,
            vertexBytes,
            indexBytes
        };
    }

    return std::nullopt;
}

bool GpuMemory::releaseGeometry(GeometrySlot slot) {
    if(slot >= geometry.slotCount || !bitIsSet(allocatedGeometrySlots, slot))
        return false;

    clearBit(allocatedGeometrySlots, slot);
    --allocatedGeometryCount;
    geometryCursor = std::min(geometryCursor, static_cast<std::size_t>(slot));
    return true;
}

std::optional<GpuMemory::TextureArrayAllocation>
GpuMemory::allocateTextureLayer(const TextureArrayLayout &layout) {
    if(!isValidLayout(layout))
        return std::nullopt;

    for(std::size_t poolIndex = 0; poolIndex < texturePools.size(); ++poolIndex) {
        TexturePool &pool = texturePools[poolIndex];
          if(pool.layout.width != layout.width || pool.layout.height != layout.height ||
              pool.layout.format != layout.format || pool.layout.mipLevels != layout.mipLevels ||
              pool.layout.sampleCount != layout.sampleCount || pool.layout.flags != layout.flags)
            continue;

        for(std::uint32_t layer = pool.cursor; layer < layerCapacity; ++layer) {
            if(bitIsSet(pool.allocatedLayers, layer))
                continue;

            setBit(pool.allocatedLayers, layer);
            pool.cursor = layer + 1;
            while(pool.cursor < layerCapacity && bitIsSet(pool.allocatedLayers, pool.cursor))
                ++pool.cursor;

            const bool createArray = !pool.resourceCreated && !pool.creationRequested;
            if(createArray)
                pool.creationRequested = true;

            return TextureArrayAllocation{
                static_cast<TextureArrayId>(poolIndex),
                layer,
                createArray,
                pool.layout
            };
        }
    }

    if(texturePools.size() >= std::numeric_limits<TextureArrayId>::max())
        return std::nullopt;

    TexturePool pool{
        layout,
        std::vector<std::uint64_t>(bitmapWordCount(layerCapacity)),
        1,
        false,
        true
    };
    setBit(pool.allocatedLayers, 0);
    texturePools.push_back(std::move(pool));

    return TextureArrayAllocation{
        static_cast<TextureArrayId>(texturePools.size() - 1),
        0,
        true,
        layout
    };
}

bool GpuMemory::releaseTextureLayer(TextureArrayId arrayId, TextureLayer layer) {
    if(arrayId >= texturePools.size() || layer >= layerCapacity)
        return false;

    TexturePool &pool = texturePools[arrayId];
    if(!bitIsSet(pool.allocatedLayers, layer))
        return false;

    clearBit(pool.allocatedLayers, layer);
    pool.cursor = std::min(pool.cursor, layer);
    return true;
}

bool GpuMemory::markTextureArrayCreated(TextureArrayId arrayId) {
    if(arrayId >= texturePools.size())
        return false;
    TexturePool &pool = texturePools[arrayId];
    pool.resourceCreated = true;
    pool.creationRequested = true;
    return true;
}

bool GpuMemory::markTextureArrayCreationFailed(TextureArrayId arrayId) {
    if(arrayId >= texturePools.size())
        return false;
    TexturePool &pool = texturePools[arrayId];
    if(pool.resourceCreated)
        return false;
    pool.creationRequested = false;
    return true;
}

std::size_t GpuMemory::vertexBufferSize() const noexcept {
    return geometry.slotCount * vertexStrideBytes;
}

std::size_t GpuMemory::indexBufferSize() const noexcept {
    return geometry.slotCount * indexStrideBytes;
}

std::size_t GpuMemory::vertexSlotStride() const noexcept {
    return vertexStrideBytes;
}

std::size_t GpuMemory::indexSlotStride() const noexcept {
    return indexStrideBytes;
}

std::size_t GpuMemory::geometrySlotCount() const noexcept {
    return geometry.slotCount;
}

std::size_t GpuMemory::freeGeometrySlotCount() const noexcept {
    return geometry.slotCount - allocatedGeometryCount;
}

std::uint32_t GpuMemory::maxLayersPerTextureArray() const noexcept {
    return layerCapacity;
}

std::size_t GpuMemory::textureArrayCount() const noexcept {
    return texturePools.size();
}

} // namespace nx::rhi
