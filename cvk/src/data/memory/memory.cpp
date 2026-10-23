#include "cvk/data/memory/memory.hpp"

#include "cvk/base/allocator.hpp"
#include "cvk/base/core.hpp"
#include "cvk/utility/vk_exceptions.hpp"

#include <algorithm>


namespace cvk {
Memory::Memory(Core const& core, Config const& config) : _handle{core}, _config{config} { Core::debug_check(core); }

Memory::Memory(Core const& core, Config const& config, State const& state) : Memory{core, config} { wrap(state); }

Memory::Memory(Core const& core, Config const& config, VkMemoryRequirements const& requirements) : Memory{
    core,
    config
} { create(requirements); }

void Memory::wrap(State const& state) {
    optDestroy();

    _handle.reset(state.vmaAllocation.get(), core());
}

void Memory::create(VkMemoryRequirements const& requirements) {
    CTH_CRITICAL(_config.autoUsage(), "VMA_MEMORY_USAGE_AUTO* needs a buffer or an image, use the flags") {}

    optDestroy();

    auto const info = _config.createInfo();

    VmaAllocation ptr = VK_NULL_HANDLE;
    auto const result = vmaAllocateMemory(allocator(), &requirements, &info, &ptr, nullptr);

    CTH_STABLE_ERR(result != VK_SUCCESS, "failed to allocate memory") {
        forget();
        throw cvk::vk_result_exception{result, details->exception()};
    }

    _handle.reset(ptr, core());
}

void Memory::destroy() {
    debug_check(*this);

    _handle.reset();

    forget();
}

Memory::State Memory::release() {
    debug_check(*this);

    State const state{_handle.release()};
    forget();
    return state;
}

Mapping<std::byte> Memory::map(Range range) const {
    debug_check(*this);
    CTH_CRITICAL((properties() & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) == 0, "memory must be host visible") {}

    size_t const memorySize = size();
    CTH_CRITICAL(range.offset > memorySize, "offset must lie in the memory") {}

    size_t const mapSize = range.size == VK_WHOLE_SIZE ? memorySize - range.offset : range.size;
    CTH_CRITICAL(mapSize > memorySize - range.offset, "range must lie in the memory") {}

    void* data = nullptr;
    auto const result = vmaMapMemory(allocator(), get(), &data);
    CVK_RESULT_STABLE_THROW(result != VK_SUCCESS, result, "failed to map memory") {}

    return Mapping<std::byte>{*this, {static_cast<std::byte*>(data) + range.offset, mapSize}};
}

void Memory::unmap() const {
    debug_check(*this);

    vmaUnmapMemory(allocator(), get());
}

void Memory::flush(Range range) const {
    debug_check(*this);

    auto const result = vmaFlushAllocation(allocator(), get(), range.offset, range.size);
    CVK_RESULT_STABLE_THROW(result != VK_SUCCESS, result, "failed to flush memory") {}
}

void Memory::invalidate(Range range) const {
    debug_check(*this);

    auto const result = vmaInvalidateAllocation(allocator(), get(), range.offset, range.size);
    CVK_RESULT_STABLE_THROW(result != VK_SUCCESS, result, "failed to invalidate memory") {}
}

void Memory::write(std::span<std::byte const> data, size_t offset) const {
    debug_check(*this);
    CTH_CRITICAL((properties() & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) == 0, "memory must be host visible") {}
    CTH_CRITICAL(offset > size() || data.size() > size() - offset, "range must lie in the memory") {}

    if(data.empty()) return;

    // not vmaCopyMemoryToAllocation(): it flushes after the unmap, non coherent memory must be mapped to flush
    VkResult result = VK_SUCCESS;
    {
        auto const mapping = map({.offset = offset, .size = data.size()});
        std::ranges::copy(data, mapping.begin());
        result = vmaFlushAllocation(allocator(), get(), offset, data.size());
    }

    CVK_RESULT_STABLE_THROW(result != VK_SUCCESS, result, "failed to flush memory") {}
}

void Memory::destroy(VmaAllocator vmaAllocator, VmaAllocation vmaAllocation) {
    CTH_WARN(vmaAllocation == VK_NULL_HANDLE, "vmaAllocation invalid (VK_NULL_HANDLE)") {}

    vmaFreeMemory(vmaAllocator, vmaAllocation);
}

VmaAllocationInfo Memory::info() const {
    debug_check(*this);

    VmaAllocationInfo info{};
    vmaGetAllocationInfo(allocator(), get(), &info);
    return info;
}

std::span<std::byte> Memory::mapped() const {
    auto const allocationInfo = info();
    if(allocationInfo.pMappedData == nullptr) return {};

    return {static_cast<std::byte*>(allocationInfo.pMappedData), allocationInfo.size};
}

VkMemoryPropertyFlags Memory::properties() const {
    debug_check(*this);

    VkMemoryPropertyFlags flags = 0;
    vmaGetAllocationMemoryProperties(allocator(), get(), &flags);
    return flags;
}

void Memory::forget() {}

VmaAllocator Memory::allocator() const { return core().allocator().get(); }
}
