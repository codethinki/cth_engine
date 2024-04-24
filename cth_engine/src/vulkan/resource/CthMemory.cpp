#include "CthMemory.hpp"

#include "vulkan/base/CthDevice.hpp"
#include "vulkan/base/CthPhysicalDevice.hpp"
#include "vulkan/utility/CthVkUtils.hpp"

#include <cth/cth_log.hpp>

#include "CthDeletionQueue.hpp"

namespace cth {


void BasicMemory::alloc(const VkMemoryRequirements& requirements) {
    CTH_ERR(vkMemory != VK_NULL_HANDLE, "memory already allocated") throw details->exception();

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = requirements.size;
    allocInfo.memoryTypeIndex = device->physical()->findMemoryType(requirements.memoryTypeBits, vkProperties);

    const VkResult allocResult = vkAllocateMemory(device->get(), &allocInfo, nullptr, &vkMemory);

    CTH_STABLE_ERR(allocResult != VK_SUCCESS, "failed to allocate buffer memory")
        throw cth::except::vk_result_exception{allocResult, details->exception()};

    _size = requirements.size;
}
span<char> BasicMemory::map(const size_t map_size, const size_t offset) const {
    CTH_ERR(vkMemory == VK_NULL_HANDLE, "memory not allocated");

    void* mappedPtr = nullptr;
    const VkResult mapResult = vkMapMemory(device->get(), vkMemory, offset, _size, 0, &mappedPtr);
    CTH_STABLE_ERR(mapResult != VK_SUCCESS, "memory mapping failed")
        throw except::vk_result_exception{mapResult, details->exception()};

    return span<char>{static_cast<char*>(mappedPtr), map_size};
}
VkResult BasicMemory::flush(const size_t size, const size_t offset) const {
    VkMappedMemoryRange mappedRange = {};
    mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    mappedRange.memory = vkMemory;
    mappedRange.offset = offset;
    mappedRange.size = size;
    return vkFlushMappedMemoryRanges(device->get(), 1, &mappedRange);
}
VkResult BasicMemory::invalidate(const size_t size, const size_t offset) const {
    VkMappedMemoryRange mappedRange = {};
    mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    mappedRange.memory = vkMemory;
    mappedRange.offset = offset;
    mappedRange.size = size;
    return vkInvalidateMappedMemoryRanges(device->get(), 1, &mappedRange);
}
void BasicMemory::unmap() const { vkUnmapMemory(device->get(), vkMemory); }

void BasicMemory::free(DeletionQueue* deletion_queue) {
    CTH_WARN(vkMemory == VK_NULL_HANDLE, "memory not valid");
    if(!deletion_queue) free(device, vkMemory);
    else deletion_queue->push(vkMemory);
    reset();
}
void BasicMemory::free(const Device* device, VkDeviceMemory memory) {
    CTH_WARN(memory == VK_NULL_HANDLE, "memory not allocated");
    vkFreeMemory(device->get(), memory, nullptr);
}
void BasicMemory::reset() {
    vkMemory = VK_NULL_HANDLE;
    _size = 0;
}


#ifdef _DEBUG
void BasicMemory::debug_check(const BasicMemory* memory) {
    CTH_ERR(memory == nullptr, "memory must not be nullptr") throw details->exception();
    CTH_ERR(memory->vkMemory == VK_NULL_HANDLE, "memory must be a valid handle") throw details->exception();
}
#endif
} // namespace cth

//Memory

namespace cth {
Memory::~Memory() {
    if(get() != VK_NULL_HANDLE) Memory::free();
}
void Memory::free(DeletionQueue* deletion_queue) {
    if(!deletion_queue) deletionQueue->push(get());
    else {
        deletion_queue->push(get());
        deletionQueue = deletion_queue;
    }
    reset();
}
}
