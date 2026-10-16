#include "cvk/data/buffer/buffer.hpp"

#include "cvk/base/allocator.hpp"
#include "cvk/base/core.hpp"
#include "cvk/base/physical_device.hpp"
#include "cvk/utility/vk_exceptions.hpp"

#include <algorithm>


namespace cvk {
Buffer::Buffer(Core const& core, Config const& config) : _handle{core}, _memory{core, config.memory},
    _config{config} {
    Core::debug_check(core);
    Config::debug_check(config);
    CTH_CRITICAL(
        std::ranges::any_of(
            config.queueFamilies,
            [familyCount = core.physicalDevice().queueFamilies().size()](queue_family_index_t family) {
                return family >= familyCount;
            }
        ),
        "queue families must exist on the physical device"
    ) {}
}

Buffer::Buffer(Core const& core, Config const& config, State const& state) : Buffer{core, config} { wrap(state); }

Buffer::Buffer(Core const& core, Config const& config, size_t size) : Buffer{core, config} { create(size); }

void Buffer::wrap(State const& state) {
    optDestroy();

    _handle.reset(state.vkBuffer.get(), core());
    if(state.memory.has_value()) _memory.wrap(*state.memory);
    _size = state.size;
}

void Buffer::create(size_t size) {
    optDestroy();

    auto const bufferInfo = _config.createInfo(size);
    auto const allocationInfo = _memory.config().createInfo();

    VkBuffer vkBuffer = VK_NULL_HANDLE;
    VmaAllocation vmaAllocation = VK_NULL_HANDLE;
    auto const result = vmaCreateBuffer(
        core().allocator().get(),
        &bufferInfo,
        &allocationInfo,
        &vkBuffer,
        &vmaAllocation,
        nullptr
    );

    CTH_STABLE_ERR(result != VK_SUCCESS, "failed to create buffer") {
        forget();
        throw cvk::vk_result_exception{result, details->exception()};
    }

    _handle.reset(vkBuffer, core());
    _memory.wrap(Memory::State{vmaAllocation});
    _size = size;
}

void Buffer::destroy() {
    debug_check(*this);

    _handle.reset();
    _memory.optDestroy();

    forget();
}

Buffer::State Buffer::release() {
    debug_check(*this);

    State state{.vkBuffer = _handle.release(), .size = _size, .memory = std::nullopt};
    if(_memory.created()) state.memory = _memory.release();

    forget();
    return state;
}

void Buffer::destroy(DeviceTable table, VkBuffer vkBuffer) {
    CTH_WARN(vkBuffer == VK_NULL_HANDLE, "vkBuffer invalid (VK_NULL_HANDLE)") {}

    table->vkDestroyBuffer(table.device(), vkBuffer, nullptr);
}

void Buffer::forget() { _size = 0; }
}
