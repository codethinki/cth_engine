#include "CthMemory.hpp"

#include "vulkan/base/CthDevice.hpp"
#include "vulkan/base/CthPhysicalDevice.hpp"
#include "vulkan/resource/CthDeletionQueue.hpp"
#include "vulkan/utility/CthVkUtils.hpp"


#include <cth/cth_log.hpp>



namespace cth {


void BasicMemory::alloc(const VkMemoryRequirements& requirements) {
    CTH_ERR(_vkMemory != VK_NULL_HANDLE, "memory already allocated") throw details->exception();

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = requirements.size;
    allocInfo.memoryTypeIndex = _device->physical()->findMemoryType(requirements.memoryTypeBits, _vkProperties);

    const VkResult allocResult = vkAllocateMemory(_device->get(), &allocInfo, nullptr, &_vkMemory);

    CTH_STABLE_ERR(allocResult != VK_SUCCESS, "failed to allocate buffer memory")
        throw cth::except::vk_result_exception{allocResult, details->exception()};

    _size = requirements.size;
}
span<char> BasicMemory::map(const size_t map_size, const size_t offset) const {
    CTH_ERR(_vkMemory == VK_NULL_HANDLE, "memory not allocated");

    void* mappedPtr = nullptr;
    const VkResult mapResult = vkMapMemory(_device->get(), _vkMemory, offset, _size, 0, &mappedPtr);
    CTH_STABLE_ERR(mapResult != VK_SUCCESS, "memory mapping failed")
        throw except::vk_result_exception{mapResult, details->exception()};

    return span<char>{static_cast<char*>(mappedPtr), map_size};
}
VkResult BasicMemory::flush(const size_t size, const size_t offset) const {
    VkMappedMemoryRange mappedRange = {};
    mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    mappedRange.memory = _vkMemory;
    mappedRange.offset = offset;
    mappedRange.size = size;
    return vkFlushMappedMemoryRanges(_device->get(), 1, &mappedRange);
}
VkResult BasicMemory::invalidate(const size_t size, const size_t offset) const {
    VkMappedMemoryRange mappedRange = {};
    mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    mappedRange.memory = _vkMemory;
    mappedRange.offset = offset;
    mappedRange.size = size;
    return vkInvalidateMappedMemoryRanges(_device->get(), 1, &mappedRange);
}
void BasicMemory::unmap() const { vkUnmapMemory(_device->get(), _vkMemory); }

void BasicMemory::free(DeletionQueue* deletion_queue) {
    CTH_WARN(_vkMemory == VK_NULL_HANDLE, "memory not valid");
    if(!deletion_queue) free(_device, _vkMemory);
    else deletion_queue->push(_vkMemory);
    reset();
}
void BasicMemory::free(const Device* device, VkDeviceMemory memory) {
    CTH_WARN(memory == VK_NULL_HANDLE, "memory not allocated");
    vkFreeMemory(device->get(), memory, nullptr);
}
void BasicMemory::reset() {
    _vkMemory = VK_NULL_HANDLE;
    _size = 0;
}


#ifdef _DEBUG
void BasicMemory::debug_check(const BasicMemory* memory) {
    CTH_ERR(memory == nullptr, "memory must not be nullptr") throw details->exception();
    CTH_ERR(memory->_vkMemory == VK_NULL_HANDLE, "memory must be a valid handle") throw details->exception();
}
#endif
} // namespace cth

//Memory

namespace cth {
Memory::~Memory() { if(get() != VK_NULL_HANDLE) Memory::free(); }

void Memory::alloc(const VkMemoryRequirements& requirements) {
    if(get() != VK_NULL_HANDLE) Memory::free();
    BasicMemory::alloc(requirements);
}
void Memory::free(DeletionQueue* deletion_queue) {
    if(deletion_queue) _deletionQueue = deletion_queue;
    BasicMemory::free(_deletionQueue);
}
}
