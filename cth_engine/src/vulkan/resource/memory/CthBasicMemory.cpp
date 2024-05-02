#include "CthBasicMemory.hpp"

#include "vulkan/base/CthDevice.hpp"
#include "vulkan/base/CthPhysicalDevice.hpp"
#include "vulkan/resource/CthDeletionQueue.hpp"
#include "vulkan/utility/CthVkUtils.hpp"


#include <cth/cth_log.hpp>



namespace cth {


BasicMemory::BasicMemory(Device* device, const VkMemoryPropertyFlags vk_properties) : _device(device), _vkProperties(vk_properties) { init(); }
BasicMemory::BasicMemory(Device* device, const VkMemoryPropertyFlags properties, const size_t byte_size, VkDeviceMemory vk_memory) : _device(device),
    _vkProperties(properties), _size(byte_size), _handle(vk_memory) {
    init();
    BasicMemory::wrap(vk_memory, byte_size);
}

void BasicMemory::wrap(VkDeviceMemory vk_memory, const size_t byte_size) {
    DEBUG_CHECK_MEMORY_LEAK(this);

    _handle = vk_memory;
    _size = byte_size;
}

void BasicMemory::alloc(const VkMemoryRequirements& vk_requirements) {
    DEBUG_CHECK_MEMORY_LEAK(this);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = vk_requirements.size;
    allocInfo.memoryTypeIndex = _device->physical()->findMemoryType(vk_requirements.memoryTypeBits, _vkProperties);

    VkDeviceMemory ptr = VK_NULL_HANDLE;

    const VkResult allocResult = vkAllocateMemory(_device->get(), &allocInfo, nullptr, &ptr);
    CTH_STABLE_ERR(allocResult != VK_SUCCESS, "failed to allocate buffer memory")
        throw cth::except::vk_result_exception{allocResult, details->exception()};

    _handle = ptr;

    _size = vk_requirements.size;
}
span<char> BasicMemory::map(const size_t map_size, const size_t offset) const {
    CTH_ERR(_handle == VK_NULL_HANDLE, "memory not allocated");

    void* mappedPtr = nullptr;
    const VkResult mapResult = vkMapMemory(_device->get(), _handle.get(), offset, _size, 0, &mappedPtr);
    CTH_STABLE_ERR(mapResult != VK_SUCCESS, "memory mapping failed")
        throw except::vk_result_exception{mapResult, details->exception()};

    return span<char>{static_cast<char*>(mappedPtr), map_size};
}
VkResult BasicMemory::flush(const size_t size, const size_t offset) const {
    VkMappedMemoryRange mappedRange = {};
    mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    mappedRange.memory = _handle.get();
    mappedRange.offset = offset;
    mappedRange.size = size;
    return vkFlushMappedMemoryRanges(_device->get(), 1, &mappedRange);
}
VkResult BasicMemory::invalidate(const size_t size, const size_t offset) const {
    VkMappedMemoryRange mappedRange = {};
    mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    mappedRange.memory = _handle.get();
    mappedRange.offset = offset;
    mappedRange.size = size;
    return vkInvalidateMappedMemoryRanges(_device->get(), 1, &mappedRange);
}
void BasicMemory::unmap() const { vkUnmapMemory(_device->get(), _handle.get()); }

void BasicMemory::free(DeletionQueue* deletion_queue) {
    CTH_WARN(_handle == VK_NULL_HANDLE, "memory not valid");
    if(deletion_queue) deletion_queue->push(_handle.get());
    else BasicMemory::free(_device, _handle.get());

    _handle = VK_NULL_HANDLE;
    BasicMemory::reset();
}

void BasicMemory::reset() {
    DEBUG_CHECK_MEMORY_LEAK(this);

    _handle = VK_NULL_HANDLE;
    _size = 0;
}

void BasicMemory::free(const Device* device, VkDeviceMemory memory) {
    DEBUG_CHECK_DEVICE(device);
    CTH_WARN(memory == VK_NULL_HANDLE, "memory not allocated");
    vkFreeMemory(device->get(), memory, nullptr);
}


void BasicMemory::init() const { DEBUG_CHECK_DEVICE(_device); }


#ifdef _DEBUG
void BasicMemory::debug_check(const BasicMemory* memory) {
    CTH_ERR(memory == nullptr, "memory must not be nullptr") throw details->exception();
    CTH_ERR(memory->_handle == VK_NULL_HANDLE, "memory must be a valid handle") throw details->exception();
}
void BasicMemory::debug_check_leak(const BasicMemory* memory) {
    CTH_WARN(memory->_handle != VK_NULL_HANDLE, "memory handle replaced (potential memory leak)");
}
#endif
} // namespace cth


