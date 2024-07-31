#include "CthFence.hpp"

#include "vulkan/base/CthCore.hpp"
#include "vulkan/base/CthDevice.hpp"
#include "vulkan/resource/CthDeletionQueue.hpp"
#include "vulkan/utility/cth_vk_utils.hpp"



namespace cth::vk {

BasicFence::BasicFence(BasicCore const* core) : _core(core) { DEBUG_CHECK_CORE(core); }

void BasicFence::wrap(VkFence vk_fence) {
    CTH_ERR(vk_fence == VK_NULL_HANDLE, "fence handle invalid") throw details->exception();
    DEBUG_CHECK_FENCE_LEAK(this);

    _handle = vk_fence;
}

void BasicFence::create(VkFenceCreateFlags const flags) {
    DEBUG_CHECK_FENCE_LEAK(this);

    auto const info = createInfo(flags);

    VkFence ptr = VK_NULL_HANDLE;
    auto const result = vkCreateFence(_core->vkDevice(), &info, nullptr, &ptr);

    CTH_STABLE_ERR(result != VK_SUCCESS, "failed to create fence")
        throw cth::except::vk_result_exception{result, details->exception()};

    _handle = ptr;
}
void BasicFence::destroy(DeletionQueue* deletion_queue) {
    if(deletion_queue) {
        DEBUG_CHECK_DELETION_QUEUE(deletion_queue);
        deletion_queue->push(_handle.get());
    } else destroy(_core->vkDevice(), _handle.get());

    _handle = VK_NULL_HANDLE;
}
VkResult BasicFence::status() const {
    DEBUG_CHECK_FENCE(this);

    return vkGetFenceStatus(_core->vkDevice(), _handle.get());
}
void BasicFence::reset() const {
    DEBUG_CHECK_FENCE(this);
    std::array<VkFence, 1> const fences = {_handle.get()};
    auto const result = vkResetFences(_core->vkDevice(), static_cast<uint32_t>(fences.size()), fences.data());

    CTH_STABLE_ERR(result != VK_SUCCESS, "failed to reset fence")
        throw cth::except::vk_result_exception{result, details->exception()};
}


VkResult BasicFence::wait(uint64_t const timeout) const {
    DEBUG_CHECK_FENCE(this);

    std::array<VkFence, 1> const fences = {_handle.get()};
    VkResult const result = vkWaitForFences(_core->vkDevice(), static_cast<uint32_t>(fences.size()), fences.data(), VK_TRUE, timeout);

    CTH_STABLE_ERR(result != VK_SUCCESS && result != VK_TIMEOUT, "failed to wait for fence")
        throw cth::except::vk_result_exception{result, details->exception()};

    return result;
}
void BasicFence::wait() const {
    // ReSharper disable once CppExpressionWithoutSideEffects
    wait(std::numeric_limits<uint64_t>::max());
}


void BasicFence::destroy(VkDevice vk_device, VkFence vk_fence) {
    DEBUG_CHECK_DEVICE_HANDLE(vk_device);
    CTH_WARN(vk_fence == VK_NULL_HANDLE, "handle invalid") throw details->exception();

    vkDestroyFence(vk_device, vk_fence, nullptr);
}

VkFenceCreateInfo BasicFence::createInfo(VkFenceCreateFlags const flags) {
    return VkFenceCreateInfo{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = flags,
    };
}


void BasicFence::debug_check(BasicFence const* fence) {
    CTH_ERR(fence == nullptr, "fence must not be nullptr") throw details->exception();
    CTH_ERR(fence->_handle == VK_NULL_HANDLE, "fence handle invalid") throw details->exception();
}
void BasicFence::debug_check_leak(BasicFence const* fence) { CTH_WARN(fence->_handle != VK_NULL_HANDLE, "fence replaced (potential memory leak)") {} }

}

//Fence

namespace cth::vk {
Fence::Fence(BasicCore const* core, DeletionQueue* deletion_queue, VkFenceCreateFlags const flags) : BasicFence(core), _deletionQueue(deletion_queue) {
    DEBUG_CHECK_DELETION_QUEUE_NULL_ALLOWED(deletion_queue);
    BasicFence::create(flags);
}
Fence::~Fence() { if(get() != VK_NULL_HANDLE) Fence::destroy(); }

void Fence::wrap(VkFence vk_fence) {
    if(get() != VK_NULL_HANDLE) destroy(_deletionQueue);
    BasicFence::wrap(vk_fence);
}
void Fence::create(VkFenceCreateFlags const flags) {
    if(get() != VK_NULL_HANDLE) destroy(_deletionQueue);
    BasicFence::create(flags);
}
void Fence::destroy(DeletionQueue* deletion_queue) {
    if(deletion_queue) _deletionQueue = deletion_queue;
    BasicFence::destroy(_deletionQueue);
}

}
