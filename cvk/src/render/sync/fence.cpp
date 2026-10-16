#include "cvk/render/sync/fence.hpp"

#include "cvk/base/core.hpp"
#include "cvk/utility/vk_exceptions.hpp"

#include <limits>


namespace cvk {
Fence::Fence(Core const& core) : _deviceTable{core.deviceTable()} {}

Fence::Fence(Core const& core, State const& state) : Fence{core} { wrap(state); }

Fence::Fence(Core const& core, VkFenceCreateFlags flags) : Fence{core} { create(flags); }

void Fence::wrap(State const& state) {
    optDestroy();

    _handle = state.vkFence.get();
}

void Fence::create(VkFenceCreateFlags flags) {
    optDestroy();

    auto const info = createInfo(flags);

    VkFence ptr = VK_NULL_HANDLE;
    auto const result = _deviceTable->vkCreateFence(_deviceTable.device(), &info, nullptr, &ptr);

    CTH_STABLE_ERR(result != VK_SUCCESS, "failed to create fence") {
        resetState();
        throw cvk::vk_result_exception{result, details->exception()};
    }

    _handle = ptr;
}

void Fence::destroy() {
    debug_check(*this);

    destroy(_deviceTable, _handle.get());

    resetState();
}

Fence::State Fence::release() {
    debug_check(*this);

    State const state{_handle.get()};
    resetState();
    return state;
}

VkResult Fence::status() const {
    debug_check(*this);

    auto const result = _deviceTable->vkGetFenceStatus(_deviceTable.device(), _handle.get());

    CVK_RESULT_STABLE_THROW(
        result != VK_SUCCESS && result != VK_NOT_READY,
        result,
        "failed to get fence status"
    ) {}

    return result;
}

void Fence::reset() const {
    debug_check(*this);

    auto const vkFence = _handle.get();
    auto const result = _deviceTable->vkResetFences(_deviceTable.device(), 1, &vkFence);

    CVK_RESULT_STABLE_THROW(result != VK_SUCCESS, result, "failed to reset fence") {}
}

VkResult Fence::wait(uint64_t timeout) const {
    debug_check(*this);

    auto const vkFence = _handle.get();
    auto const result = _deviceTable->vkWaitForFences(_deviceTable.device(), 1, &vkFence, VK_TRUE, timeout);

    CVK_RESULT_STABLE_THROW(result != VK_SUCCESS && result != VK_TIMEOUT, result, "failed to wait for fence") {}

    return result;
}

void Fence::wait() const { [[maybe_unused]] auto const result = wait(std::numeric_limits<uint64_t>::max()); }

VkResult Fence::waitReset(uint64_t timeout) const {
    auto const result = wait(timeout);
    if(result == VK_TIMEOUT) return result;

    reset();
    return result;
}

void Fence::waitReset() const {
    wait();
    reset();
}

void Fence::destroy(DeviceTable table, VkFence vkFence) {
    CTH_WARN(vkFence == VK_NULL_HANDLE, "vkFence invalid (VK_NULL_HANDLE)") {}

    table->vkDestroyFence(table.device(), vkFence, nullptr);
}

void Fence::resetState() { _handle = VK_NULL_HANDLE; }

VkFenceCreateInfo Fence::createInfo(VkFenceCreateFlags flags) {
    return VkFenceCreateInfo{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = flags,
    };
}
}
