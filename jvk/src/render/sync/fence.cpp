#include "jvk/render/sync/fence.hpp"

#include "jvk/base/core.hpp"
#include "jvk/base/device.hpp"
#include "jvk/res/destruction_queue.hpp"
#include "jvk/utility/vk_exceptions.hpp"
#include "jvk/utility/os/os_constants.hpp"

namespace jvk {

jvk::Fence::Fence(Core const& core) : _core{&core} { Core::debug_check(core); }
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
        throw jvk::vk_result_exception{result, details->exception()};

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
        throw jvk::vk_result_exception{result, details->exception()};

    return result;
}

void Fence::reset() const {
    debug_check(this);
    std::array<VkFence, 1> const fences = {_handle.get()};
    auto const result = _core->functions()->vkResetFences(
        _core->vkDevice(),
        static_cast<uint32_t>(fences.size()),
        fences.data()
    );

    CTH_STABLE_ERR(result != VK_SUCCESS, "failed to reset fence")
        throw jvk::vk_result_exception{result, details->exception()};
}


VkResult Fence::wait(wait_t timeout) const {
    debug_check(this);

    std::array const fences = {_handle.get()};


    auto const result = _core->functions()->vkWaitForFences(
        _core->vkDevice(),
        static_cast<uint32_t>(fences.size()),
        fences.data(),
        VK_TRUE,
        timeout
    );

    CTH_STABLE_ERR(result != VK_SUCCESS && result != VK_TIMEOUT, "failed to wait for fence")
        throw jvk::vk_result_exception{result, details->exception()};

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

namespace os {
    consteval auto get_fence_handle_info() {
        if constexpr(PLATFORM == Platform::WINDOWS)
            return VkFenceGetWin32HandleInfoKHR{
                VK_STRUCTURE_TYPE_FENCE_GET_WIN32_HANDLE_INFO_KHR,
                nullptr,
                VK_NULL_HANDLE,
                VK_EXTERNAL_FENCE_HANDLE_TYPE_OPAQUE_WIN32_BIT
            };
        else if constexpr(PLATFORM == Platform::LINUX)
            return VkFenceGetFdInfoKHR{
                VK_STRUCTURE_TYPE_FENCE_GET_WIN32_HANDLE_INFO_KHR,
                nullptr,
                VK_NULL_HANDLE,
                VK_EXTERNAL_FENCE_HANDLE_TYPE_OPAQUE_FD_BIT
            };
        else return std::type_identity<void>{};
    }

}



void* Fence::extractOsHandle() const {
    debug_check(this);

    auto const info = os::get_fence_handle_info();


    os::fence_handle_t handle = nullptr;
    VkResult result = VK_RESULT_MAX_ENUM;
    if constexpr(jvk::os::PLATFORM == jvk::os::Platform::WINDOWS)
        result = _core->functions()->vkGetFenceWin32HandleKHR(
            _core->vkDevice(),
            reinterpret_cast<VkFenceGetWin32HandleInfoKHR const*>(&info),
            static_cast<HANDLE*>(handle)
        );
    else
        result = _core->functions()->vkGetFenceFdKHR(
            _core->vkDevice(),
            reinterpret_cast<VkFenceGetFdInfoKHR const*>(&info),
            static_cast<int*>(handle)
        );

    JVK_RESULT_STABLE_THROW(result != VK_SUCCESS, result, "failed to extract fence");

    return handle;
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
