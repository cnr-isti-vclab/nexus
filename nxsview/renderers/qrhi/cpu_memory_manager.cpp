#include "cpu_memory_manager.h"

#include <cstdint>
#include <new>

namespace nx::rhi {

CpuMemory::CpuMemory(std::size_t maximumBytes)
    : maximumBytes(maximumBytes) {}

CpuMemory::~CpuMemory() {
    std::lock_guard<std::mutex> lock(mutex);
    for(const auto &[memory, size] : allocations) {
        (void)size;
        delete[] static_cast<std::byte *>(memory);
    }
}

void *CpuMemory::allocate(std::size_t bytes) {
    if(bytes == 0)
        return nullptr;

    {
        std::lock_guard<std::mutex> lock(mutex);
        if(bytes > maximumBytes - usedBytes)
            return nullptr;
        usedBytes += bytes;
    }

    std::byte *memory = new (std::nothrow) std::byte[bytes];
    if(!memory) {
        std::lock_guard<std::mutex> lock(mutex);
        usedBytes -= bytes;
        return nullptr;
    }

    try {
        std::lock_guard<std::mutex> lock(mutex);
        allocations.emplace(memory, bytes);
    } catch(...) {
        delete[] memory;
        std::lock_guard<std::mutex> lock(mutex);
        usedBytes -= bytes;
        throw;
    }
    return memory;
}

bool CpuMemory::deallocate(void *memory) {
    if(!memory)
        return false;

    {
        std::lock_guard<std::mutex> lock(mutex);
        const auto found = allocations.find(memory);
        if(found == allocations.end())
            return false;
        usedBytes -= found->second;
        allocations.erase(found);
    }

    delete[] static_cast<std::byte *>(memory);
    return true;
}

bool CpuMemory::setMaximum(std::size_t maximumBytes) {
    std::lock_guard<std::mutex> lock(mutex);
    if(maximumBytes < usedBytes)
        return false;
    this->maximumBytes = maximumBytes;
    return true;
}

std::size_t CpuMemory::maximum() const {
    std::lock_guard<std::mutex> lock(mutex);
    return maximumBytes;
}

std::size_t CpuMemory::used() const {
    std::lock_guard<std::mutex> lock(mutex);
    return usedBytes;
}

std::size_t CpuMemory::available() const {
    std::lock_guard<std::mutex> lock(mutex);
    return maximumBytes - usedBytes;
}

} // namespace nx::rhi
