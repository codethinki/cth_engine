#include "CthFence.hpp"

#include "vulkan/base/CthCore.hpp"
#include "vulkan/base/CthDevice.hpp"
#include "vulkan/resource/CthDestructionQueue.hpp"
#include "vulkan/utility/cth_vk_exceptions.hpp"



namespace cth::vk {

cth::vk::Fence::Fence(cth::not_null<Core const*> core) : _core(core) { Core::debug_check(core); }
Fence::Fence(cth::not_null<Core const*> core, State const& state) : Fence{core} { wrap(state); }
Fence::Fence(cth::not_null<Core const*> core, VkFenceCreateFlags flags) : Fence{core} { create(flags); }

void Fence::wrap(State const& state) {
    optDestroy();

    _handle = state.vkFence.get();
}

void Fence::create(VkFenceCreateFlags flags) {
    optDestroy();

    auto const info = createInfo(flags);

    VkFence ptr = VK_NULL_HANDLE;
    auto const result = _core->deviceTable()->vkCreateFence(_core->vkDevice(), &info, nullptr, &ptr);

    CTH_STABLE_ERR(result != VK_SUCCESS, "failed to create fence")
        throw cth::vk::result_exception{result, details->exception()};

    _handle = ptr;
}
void Fence::destroy() {
    debug_check(this);
    auto const lambda = [vk_device = _core->vkDevice(), vk_fence = _handle.get()] { destroy(vk_device, vk_fence); };


    auto const queue = _core->destructionQueue();
    if(queue) queue->push(lambda);
    else lambda();


    resetState();
}
VkResult Fence::status() const {
    debug_check(this);

    auto const result = vkGetFenceStatus(_core->vkDevice(), _handle.get());

    CTH_STABLE_ERR(result != VK_SUCCESS && result != VK_NOT_READY, "failed to get fence status")
        throw vk::result_exception{result, details->exception()};

    return result;
}
void Fence::reset() const {
    debug_check(this);
    std::array<VkFence, 1> const fences = {_handle.get()};
    auto const result = vkResetFences(_core->vkDevice(), static_cast<uint32_t>(fences.size()), fences.data());

    CTH_STABLE_ERR(result != VK_SUCCESS, "failed to reset fence")
        throw cth::vk::result_exception{result, details->exception()};
}


VkResult Fence::wait(uint64_t timeout) const {
    debug_check(this);

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

}
