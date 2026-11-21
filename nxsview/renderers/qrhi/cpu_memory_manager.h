#ifndef NX_RHI_CPU_MEMORY_MANAGER_H
#define NX_RHI_CPU_MEMORY_MANAGER_H

#include <cstddef>
#include <mutex>
#include <unordered_map>

namespace nx::rhi {

/**
 * Tracks caller-owned CPU allocations against a byte capacity.
 *
 * The current implementation uses new[]/delete[]. `used()` counts requested
 * payload bytes (not allocator bookkeeping or alignment overhead). Calls are
 * thread-safe. Any allocations not explicitly deallocated are reclaimed when
 * the manager is destroyed; callers must not use those pointers afterward.
 */
class CpuMemory {
public:
    explicit CpuMemory(std::size_t maximumBytes);
    ~CpuMemory();

    CpuMemory(const CpuMemory &) = delete;
    CpuMemory &operator=(const CpuMemory &) = delete;
    CpuMemory(CpuMemory &&) = delete;
    CpuMemory &operator=(CpuMemory &&) = delete;

    /** Allocate bytes, or return nullptr if capacity is insufficient or
     * allocation fails. A zero-byte request returns nullptr. */
    void *allocate(std::size_t bytes);

    /** Free a pointer previously returned by allocate(). Returns false for
     * nullptr, unknown pointers, and already-freed pointers. */
    bool deallocate(void *memory);

    /** Change capacity only when it is at least the currently used size. */
    bool setMaximum(std::size_t maximumBytes);

    std::size_t maximum() const;
    std::size_t used() const;
    std::size_t available() const;

private:
    mutable std::mutex mutex;
    std::unordered_map<void *, std::size_t> allocations;
    std::size_t maximumBytes;
    std::size_t usedBytes = 0;
};

} // namespace nx::rhi

#endif // NX_RHI_CPU_MEMORY_MANAGER_H
