#include "cvk/render/cmd/cmd_pool.hpp"

#include "cvk/base/core.hpp"
#include "cvk/utility/vk_exceptions.hpp"


namespace cvk {
CmdPool::CmdPool(Core const& core, Config const& config) : _handle{core}, _config{config} {
    Core::debug_check(core);
}

CmdPool::CmdPool(Core const& core, Config const& config, State const& state) : CmdPool{core, config} { wrap(state); }

CmdPool::CmdPool(Core const& core, Config const& config, create_t) : CmdPool{core, config} { create(); }

void CmdPool::wrap(State const& state) {
    optDestroy();

    _handle.reset(state.vkPool.get(), core());
}

void CmdPool::create() {
    CTH_CRITICAL(_config.queueFamilyIndex == VK_QUEUE_FAMILY_IGNORED, "the config needs a queue family") {}

    optDestroy();

    auto const info = _config.createInfo();

    VkCommandPool ptr = VK_NULL_HANDLE;
    auto const result = core().functions()->vkCreateCommandPool(core().vkDevice(), &info, nullptr, &ptr);

    CTH_STABLE_ERR(result != VK_SUCCESS, "failed to create command pool") {
        forget();
        throw cvk::vk_result_exception{result, details->exception()};
    }

    _handle.reset(ptr, core());
}

void CmdPool::destroy() {
    debug_check(*this);
    CTH_CRITICAL(!unused(), "command buffers of the pool must be destroyed first, got {}", _buffers) {}

    _handle.reset();

    forget();
}

CmdPool::State CmdPool::release() {
    debug_check(*this);
    CTH_CRITICAL(!unused(), "command buffers of the pool must be destroyed first, got {}", _buffers) {}

    State const state{_handle.release()};
    forget();
    return state;
}

void CmdPool::reset(VkCommandPoolResetFlags flags) const {
    debug_check(*this);

    auto const result = core().functions()->vkResetCommandPool(core().vkDevice(), get(), flags);
    CVK_RESULT_STABLE_THROW(result != VK_SUCCESS, result, "failed to reset command pool") {}

    ++_resets;
}

void CmdPool::add() const { ++_buffers; }

void CmdPool::remove() const {
    CTH_CRITICAL(unused(), "the pool counts no command buffer") {}
    --_buffers;
}

void CmdPool::destroy(DeviceTable table, VkCommandPool vkPool) {
    CTH_WARN(vkPool == VK_NULL_HANDLE, "vkPool invalid (VK_NULL_HANDLE)") {}

    table->vkDestroyCommandPool(table.device(), vkPool, nullptr);
}

void CmdPool::forget() {}
}
