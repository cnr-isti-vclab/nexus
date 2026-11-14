#ifndef NX_RHI_GPU_CACHE_ALLOCATOR_H
#define NX_RHI_GPU_CACHE_ALLOCATOR_H

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace nx::rhi {

/**
 * Logical allocator for the RHI viewer's geometry buffers and texture arrays.
 *
 * This class does not issue QRhi calls: QRhi resources must be created,
 * uploaded, and destroyed on the thread that owns the QRhi instance. The
 * returned buffer sizes and texture-array descriptions are intended to drive
 * those operations on that thread. This logical allocator is not thread-safe.
 */
class GpuMemory {
public:
    using TextureArrayId = std::uint32_t;
    using GeometrySlot = std::uint32_t;
    using TextureLayer = std::uint32_t;

    struct GeometryConfig {
        std::size_t slotCount = 0;
        std::size_t maxVertexBytesPerSlot = 0;
        std::size_t maxIndexBytesPerSlot = 0;
        std::size_t vertexAlignment = 4;
        std::size_t indexAlignment = 4;
    };

    struct GeometryAllocation {
        GeometrySlot slot;
        std::size_t vertexOffset;
        std::size_t indexOffset;
        std::size_t vertexBytes;
        std::size_t indexBytes;
    };

    /** QRhi-independent layout data needed to create one 2D texture array. */
    struct TextureArrayLayout {
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        std::int32_t  format = 0;
        std::uint32_t mipLevels = 1;
        std::uint32_t sampleCount = 1;
        std::uint32_t flags = 0;

        bool operator==(const TextureArrayLayout &other) const noexcept {
            return width == other.width && height == other.height &&
                   format == other.format && mipLevels == other.mipLevels &&
                   sampleCount == other.sampleCount && flags == other.flags;
        }
    };

    struct TextureArrayAllocation {
        TextureArrayId arrayId;
        TextureLayer layer;
        /** True when the caller must create the described QRhi texture array. */
        bool createArray;
        TextureArrayLayout layout;
    };

    /**
     * @param geometry Geometry arena slot limits. A zero slot count disables
     *                  geometry allocations.
     * @param maxLayersPerTextureArray Layer capacity for each texture array;
     *                                 must be nonzero.
     * @throws std::invalid_argument for invalid capacities or alignments.
     * @throws std::overflow_error if an arena size cannot be represented.
     */
    explicit GpuMemory(GeometryConfig geometry,
                               std::uint32_t maxLayersPerTextureArray);

    GpuMemory(const GpuMemory &) = delete;
    GpuMemory &operator=(const GpuMemory &) = delete;
    GpuMemory(GpuMemory &&) = delete;
    GpuMemory &operator=(GpuMemory &&) = delete;

    /** Allocate one geometry slot, or return nullopt if no slot fits. */
    std::optional<GeometryAllocation> allocateGeometry(std::size_t vertexBytes,
                                                       std::size_t indexBytes);

    /** Release a slot previously returned by allocateGeometry(). */
    bool releaseGeometry(GeometrySlot slot);

    /** Allocate a free layer from a compatible array, creating a logical
     * array pool when needed. Returns nullopt for an invalid layout or exhausted
     * identifier space. */
    std::optional<TextureArrayAllocation> allocateTextureLayer(
        const TextureArrayLayout &layout);

    /** Release a layer previously returned by allocateTextureLayer(). */
    bool releaseTextureLayer(TextureArrayId arrayId, TextureLayer layer);

    /** Mark a logical pool ready after its QRhi texture was created. */
    bool markTextureArrayCreated(TextureArrayId arrayId);

    /** Allow a failed QRhi texture creation to be retried on the next
     * allocation from this pool. */
    bool markTextureArrayCreationFailed(TextureArrayId arrayId);

    std::size_t vertexBufferSize() const noexcept;
    std::size_t indexBufferSize() const noexcept;
    std::size_t vertexSlotStride() const noexcept;
    std::size_t indexSlotStride() const noexcept;
    std::size_t geometrySlotCount() const noexcept;
    std::size_t freeGeometrySlotCount() const noexcept;
    std::uint32_t maxLayersPerTextureArray() const noexcept;
    std::size_t textureArrayCount() const noexcept;

private:
    struct TexturePool {
        TextureArrayLayout layout;
        std::vector<std::uint64_t> allocatedLayers;
        std::uint32_t cursor = 0;
        bool resourceCreated = false;
        bool creationRequested = false;
    };

    static std::size_t alignedSize(std::size_t size, std::size_t alignment);
    static bool isValidLayout(const TextureArrayLayout &layout) noexcept;

    GeometryConfig geometry;
    std::size_t vertexStrideBytes;
    std::size_t indexStrideBytes;
    std::vector<std::uint64_t> allocatedGeometrySlots;
    std::size_t geometryCursor = 0;
    std::size_t allocatedGeometryCount = 0;
    std::uint32_t layerCapacity;
    std::vector<TexturePool> texturePools;
};

} // namespace nx::rhi

#endif // NX_RHI_GPU_CACHE_ALLOCATOR_H
