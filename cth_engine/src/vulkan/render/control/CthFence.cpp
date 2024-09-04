#include "CthFence.hpp"

#include "vulkan/base/CthCore.hpp"
#include "vulkan/base/CthDevice.hpp"
#include "vulkan/resource/CthDestructionQueue.hpp"
#include "vulkan/utility/cth_vk_utils.hpp"



namespace cth::vk {

cth::vk::Fence::Fence(cth::not_null<BasicCore const*> core) : _core(core) { DEBUG_CHECK_CORE(core); }
Fence::Fence(cth::not_null<BasicCore const*> core, State const& state) : Fence{core} { wrap(state); }
Fence::Fence(cth::not_null<BasicCore const*> core, VkFenceCreateFlags flags) : Fence{core} { create(flags); }

void Fence::wrap(State const& state) {
    optDestroy();

    _handle = state.vkFence.get();
}

void Fence::create(VkFenceCreateFlags flags) {
    optDestroy();

    auto const info = createInfo(flags);

    VkFence ptr = VK_NULL_HANDLE;
    auto const result = vkCreateFence(_core->vkDevice(), &info, nullptr, &ptr);

    CTH_STABLE_ERR(result != VK_SUCCESS, "failed to create fence")
        throw cth::vk::result_exception{result, details->exception()};

    _handle = ptr;
}
void Fence::destroy() {
    DEBUG_CHECK_FENCE(this);

    auto const queue = _core->destructionQueue();
    if(queue) queue->push(_handle.get());
    else destroy(_core->vkDevice(), _handle.get());


    resetState();
}
VkResult Fence::status() const {
    DEBUG_CHECK_FENCE(this);

    auto const result = vkGetFenceStatus(_core->vkDevice(), _handle.get());

    CTH_STABLE_ERR(result != VK_SUCCESS && result != VK_NOT_READY, "failed to get fence status")
        throw vk::result_exception{result, details->exception()};

    return result;
}
void Fence::reset() const {
    DEBUG_CHECK_FENCE(this);
    std::array<VkFence, 1> const fences = {_handle.get()};
    auto const result = vkResetFences(_core->vkDevice(), static_cast<uint32_t>(fences.size()), fences.data());

    CTH_STABLE_ERR(result != VK_SUCCESS, "failed to reset fence")
        throw cth::vk::result_exception{result, details->exception()};
}


VkResult Fence::wait(uint64_t timeout) const {
    DEBUG_CHECK_FENCE(this);

    std::array<VkFence, 1> const fences = {_handle.get()};


    VkResult const result = vkWaitForFences(_core->vkDevice(), static_cast<uint32_t>(fences.size()), fences.data(), VK_TRUE, timeout);

    CTH_STABLE_ERR(result != VK_SUCCESS && result != VK_TIMEOUT, "failed to wait for fence")
        throw cth::vk::result_exception{result, details->exception()};

    return result;
}
void Fence::wait() const {
    // ReSharper disable once CppExpressionWithoutSideEffects
    wait(std::numeric_limits<uint64_t>::max());
}


void Fence::destroy(vk::not_null<VkDevice> vk_device, VkFence vk_fence) {
    DEBUG_CHECK_DEVICE_HANDLE(vk_device);
    CTH_WARN(vk_fence == VK_NULL_HANDLE, "vk_fence should not be invalid (VK_NULL_HANDLE)") {}


    vkDestroyFence(vk_device.get(), vk_fence, nullptr);
}



void Fence::resetState() { _handle = VK_NULL_HANDLE; }

VkFenceCreateInfo Fence::createInfo(VkFenceCreateFlags flags) {
    return VkFenceCreateInfo{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = flags,
    };
}


void Fence::debug_check(Fence const* fence) {
    CTH_ERR(fence == nullptr, "fence must not be nullptr") throw details->exception();
    DEBUG_CHECK_FENCE_HANDLE(fence->_handle.get());
}
void Fence::debug_check_leak(Fence const* fence) {
    CTH_WARN(fence->_handle != VK_NULL_HANDLE, "fence replaced (potential memory leak)") {}
}
void Fence::debug_check_handle(VkFence vk_fence) {
    CTH_ERR(vk_fence == VK_NULL_HANDLE, "vk_fence handle must not be invalid (VK_NULL_HANDLE)") throw details->exception();
}

}
