#include "jvk/res/memory/memory.hpp"

#include "jvk/base/core.hpp"
#include "jvk/base/device.hpp"
#include "jvk/base/physical_device.hpp"
#include "jvk/res/destruction_queue.hpp"
#include "jvk/utility/vk_exceptions.hpp"



namespace jvk {


Memory::Memory(Core const& core, VkMemoryPropertyFlags vk_properties) : _core{&core},
    _vkProperties(vk_properties) { Core::debug_check(core); }

Memory::Memory(
    Core const& core,
    VkMemoryPropertyFlags properties,
    VkMemoryRequirements const& vk_requirements
) : Memory{core, properties} { create(vk_requirements); }

Memory::~Memory() { if(created()) Memory::destroy(); }

void Memory::wrap(State const& state) {
    if(created()) destroy();

    _handle = state.vkMemory.get();
    _size = state.size;
}

void Memory::create(VkMemoryRequirements const& vk_requirements) {
    if(created()) destroy();


    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = vk_requirements.size;
    allocInfo.memoryTypeIndex = _core->physicalDevice().findMemoryType(
        vk_requirements.memoryTypeBits,
        _vkProperties
    );

    VkDeviceMemory ptr = VK_NULL_HANDLE;

    VkResult const allocResult = _core->functions()->vkAllocateMemory(
        _core->device().get(),
        &allocInfo,
        nullptr,
        &ptr
    );

    JVK_RESULT_STABLE_THROW(allocResult != VK_SUCCESS, allocResult, "failed to allocate buffer memory");


    _handle = ptr;

    _size = vk_requirements.size;
}

std::span<char> Memory::map(size_t map_size, size_t offset) const {
    Memory::debug_check(this);

    void* mappedPtr = nullptr;
    VkResult const mapResult = _core->functions()->vkMapMemory(
        _core->vkDevice(),
        _handle.get(),
        offset,
        _size,
        0,
        &mappedPtr
    );
    JVK_RESULT_STABLE_THROW(mapResult != VK_SUCCESS, mapResult, "memory mapping failed");

    return std::span<char>{static_cast<char*>(mappedPtr), map_size};
}

void Memory::flush(size_t size, size_t offset) const {
    VkMappedMemoryRange mappedRange = {};
    mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    mappedRange.memory = _handle.get();
    mappedRange.offset = offset;
    mappedRange.size = size;
    auto const result = _core->functions()->vkFlushMappedMemoryRanges(_core->vkDevice(), 1, &mappedRange);

    CTH_STABLE_ERR(result != VK_SUCCESS, "failed to flush mapped memory ranges")
    throw jvk::vk_result_exception{result, details->exception()};
}

void Memory::invalidate(size_t size, size_t offset) const {
    VkMappedMemoryRange mappedRange = {};
    mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    mappedRange.memory = _handle.get();
    mappedRange.offset = offset;
    mappedRange.size = size;
    auto const result = _core->functions()->
                               vkInvalidateMappedMemoryRanges(_core->vkDevice(), 1, &mappedRange);

    CTH_STABLE_ERR(result != VK_SUCCESS, "failed to invalidate mapped memory ranges")
    throw jvk::vk_result_exception{result, details->exception()};
}

void Memory::unmap() const { _core->functions()->vkUnmapMemory(_core->vkDevice(), _handle.get()); }

void Memory::destroy() {
    Memory::debug_check(this);

    auto const lambda = [table = _core->deviceTable(), handle = _handle.get()] { destroy(table, handle); };

    auto const queue = _core->destructionQueue();
    if(queue) queue->push(lambda);
    else lambda();

    reset();
}


void Memory::destroy(DeviceTable table, VkDeviceMemory memory) {
    CTH_WARN(memory == VK_NULL_HANDLE, "memory handle should not be invalid (VK_NULL_HANDLE)") {}
    table->vkFreeMemory(table.device(), memory, nullptr);
}

auto Memory::release() -> State {
    Memory::debug_check(this);

    State const state{_handle.get(), _size};
    reset();
    return state;
}

void Memory::reset() {
    _handle = VK_NULL_HANDLE;
    _size = 0;
}

}
