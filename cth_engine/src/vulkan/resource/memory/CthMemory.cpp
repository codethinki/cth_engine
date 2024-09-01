#include "CthMemory.hpp"

#include "vulkan/base/CthCore.hpp"
#include "vulkan/base/CthDevice.hpp"
#include "vulkan/base/CthPhysicalDevice.hpp"
#include "vulkan/resource/CthDestructionQueue.hpp"
#include "vulkan/utility/cth_vk_utils.hpp"



namespace cth::vk {


Memory::Memory(not_null<BasicCore const*> core, VkMemoryPropertyFlags vk_properties) : _core(core), _vkProperties(vk_properties) {
    DEBUG_CHECK_CORE(_core);
}
Memory::Memory(not_null<BasicCore const*> core, VkMemoryPropertyFlags properties,
    VkMemoryRequirements const& vk_requirements) : Memory{core, properties} {
    create(vk_requirements);
}
Memory::~Memory() { if(created()) Memory::destroy(); }

void Memory::wrap(State const& state) {
    if(created()) destroy();

    _handle = state.vkMemory;
    _size = state.size;
}

void Memory::create(VkMemoryRequirements const& vk_requirements) {
    if(created()) destroy();


    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = vk_requirements.size;
    allocInfo.memoryTypeIndex = _core->physicalDevice()->findMemoryType(vk_requirements.memoryTypeBits, _vkProperties);

    VkDeviceMemory ptr = VK_NULL_HANDLE;

    VkResult const allocResult = vkAllocateMemory(_core->device()->get(), &allocInfo, nullptr, &ptr);
    CTH_STABLE_ERR(allocResult != VK_SUCCESS, "failed to allocate buffer memory")
        throw cth::except::vk_result_exception{allocResult, details->exception()};

    _handle = ptr;

    _size = vk_requirements.size;
}
std::span<char> Memory::map(size_t map_size, size_t offset) const {
    DEBUG_CHECK_MEMORY(this);

    void* mappedPtr = nullptr;
    VkResult const mapResult = vkMapMemory(_core->vkDevice(), _handle.get(), offset, _size, 0, &mappedPtr);
    CTH_STABLE_ERR(mapResult != VK_SUCCESS, "memory mapping failed")
        throw except::vk_result_exception{mapResult, details->exception()};

    return std::span<char>{static_cast<char*>(mappedPtr), map_size};
}
VkResult Memory::flush(size_t size, size_t offset) const {
    VkMappedMemoryRange mappedRange = {};
    mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    mappedRange.memory = _handle.get();
    mappedRange.offset = offset;
    mappedRange.size = size;
    return vkFlushMappedMemoryRanges(_core->vkDevice(), 1, &mappedRange);
}
VkResult Memory::invalidate(size_t size, size_t offset) const {
    VkMappedMemoryRange mappedRange = {};
    mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    mappedRange.memory = _handle.get();
    mappedRange.offset = offset;
    mappedRange.size = size;
    return vkInvalidateMappedMemoryRanges(_core->vkDevice(), 1, &mappedRange);
}
void Memory::unmap() const { vkUnmapMemory(_core->vkDevice(), _handle.get()); }

void Memory::destroy() {
    DEBUG_CHECK_MEMORY(this);

    auto const queue = _core->destructionQueue();

    if(queue) queue->push(_handle.get());
    else destroy(_core->vkDevice(), _handle.get());

    _handle = VK_NULL_HANDLE;
    _size = 0;
}


void Memory::destroy(VkDevice vk_device, VkDeviceMemory memory) {
    DEBUG_CHECK_DEVICE_HANDLE(vk_device);
    CTH_WARN(memory == VK_NULL_HANDLE, "memory handle should not be invalid (VK_NULL_HANDLE)") {}
    vkFreeMemory(vk_device, memory, nullptr);
}
auto Memory::release() -> State {
    DEBUG_CHECK_MEMORY(this);

    State const state{_handle.get(), _size};
    reset();
    return state;
}
void Memory::reset() {
    _handle = VK_NULL_HANDLE;
    _size = 0;
}


#ifdef CONSTANT_DEBUG_MODE
void Memory::debug_check(Memory const* memory) {
    CTH_ERR(memory == nullptr, "memory must not be nullptr") throw details->exception();
    CTH_ERR(!memory->created(), "memory must be created") throw details->exception();

    DEBUG_CHECK_MEMORY_HANDLE(memory->get());
}
void Memory::debug_check_handle(VkDeviceMemory vk_memory) {
    CTH_ERR(vk_memory == VK_NULL_HANDLE, "memory handle should not be invalid (VK_NULL_HANDLE)")
        throw details->exception();
}
#endif
} // namespace cth
