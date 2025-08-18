#include "vk/render/ctrl/CthFence.hpp"

#include "vk/base/CthCore.hpp"
#include "vk/base/CthDevice.hpp"
#include "vk/resource/CthDestructionQueue.hpp"
#include "vk/utility/cth_vk_exceptions.hpp"



namespace cth::vk {

cth::vk::Fence::Fence(Core const& core) : _core{&core} { Core::debug_check(core); }
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
    auto const result = _core->functions()->vkCreateFence(_core->vkDevice(), &info, nullptr, &ptr);

    CTH_STABLE_ERR(result != VK_SUCCESS, "failed to create fence")
        throw cth::vk::result_exception{result, details->exception()};

    _handle = ptr;
}
void Fence::destroy() {
    debug_check(this);
    auto const lambda = [table = _core->deviceTable(), vk_fence = _handle.get()] { destroy(table, vk_fence); };


    auto const queue = _core->destructionQueue();
    if(queue) queue->push(lambda);
    else lambda();


    resetState();
}
VkResult Fence::status() const {
    debug_check(this);

    auto const result = _core->functions()->vkGetFenceStatus(_core->vkDevice(), _handle.get());

    CTH_STABLE_ERR(result != VK_SUCCESS && result != VK_NOT_READY, "failed to get fence status")
        throw vk::result_exception{result, details->exception()};

    return result;
}
void Fence::reset() const {
    debug_check(this);
    std::array<VkFence, 1> const fences = {_handle.get()};
    auto const result = _core->functions()->vkResetFences(_core->vkDevice(), static_cast<uint32_t>(fences.size()), fences.data());

    CTH_STABLE_ERR(result != VK_SUCCESS, "failed to reset fence")
        throw cth::vk::result_exception{result, details->exception()};
}


VkResult Fence::wait(wait_t timeout) const {
    debug_check(this);

    std::array const fences = {_handle.get()};


    VkResult const result = _core->functions()->vkWaitForFences(_core->vkDevice(), static_cast<uint32_t>(fences.size()), fences.data(), VK_TRUE,
        timeout);

    CTH_STABLE_ERR(result != VK_SUCCESS && result != VK_TIMEOUT, "failed to wait for fence")
        throw cth::vk::result_exception{result, details->exception()};

    return result;
}
void Fence::waitReset() const {
    wait();
    reset();
}
VkResult Fence::waitReset(wait_t timeout) const {
    auto const result = wait(timeout);
    if(result == VK_TIMEOUT) return VK_TIMEOUT;

    reset();
    return result;
}
void Fence::wait() const { [[maybe_unused]] auto const result = wait(std::numeric_limits<uint64_t>::max()); }


void Fence::destroy(DeviceTable table, VkFence vk_fence) {
    CTH_WARN(vk_fence == VK_NULL_HANDLE, "vk_fence should not be invalid (VK_NULL_HANDLE)") {}


    table->vkDestroyFence(table.device(), vk_fence, nullptr);
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
