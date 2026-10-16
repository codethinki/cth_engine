#include "cvk/render/sync/fence.hpp"

#include "cvk/base/core.hpp"
#include "cvk/utility/constant/os_constants.hpp"
#include "cvk/utility/vk_exceptions.hpp"

#include <limits>

#ifdef CTH_PLATFORM_WINDOWS
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <vulkan/vulkan_win32.h>
#endif


namespace cvk {
Fence::Fence(Core const& core) : _handle{core} { Core::debug_check(core); }

Fence::Fence(Core const& core, State const& state) : Fence{core} { wrap(state); }

Fence::Fence(Core const& core, VkFenceCreateFlags flags, VkExternalFenceHandleTypeFlags exportTypes) : Fence{core} {
    create(flags, exportTypes);
}

void Fence::wrap(State const& state) {
    optDestroy();

    _handle.reset(state.vkFence.get(), core());
    _exportTypes = state.exportTypes;
}

void Fence::create(VkFenceCreateFlags flags, VkExternalFenceHandleTypeFlags exportTypes) {
    optDestroy();

    VkExportFenceCreateInfo const exportInfo{
        .sType = VK_STRUCTURE_TYPE_EXPORT_FENCE_CREATE_INFO,
        .pNext = nullptr,
        .handleTypes = exportTypes,
    };
    VkFenceCreateInfo const info{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = exportTypes != 0 ? &exportInfo : nullptr,
        .flags = flags,
    };

    VkFence ptr = VK_NULL_HANDLE;
    auto const result = core().functions()->vkCreateFence(core().vkDevice(), &info, nullptr, &ptr);

    CTH_STABLE_ERR(result != VK_SUCCESS, "failed to create fence") {
        resetState();
        throw cvk::vk_result_exception{result, details->exception()};
    }

    _handle.reset(ptr, core());
    _exportTypes = exportTypes;
}

void Fence::destroy() {
    debug_check(*this);

    _handle.reset();

    resetState();
}

Fence::State Fence::release() {
    debug_check(*this);

    State const state{_handle.release(), _exportTypes};
    resetState();
    return state;
}

VkResult Fence::status() const {
    debug_check(*this);

    auto const result = core().functions()->vkGetFenceStatus(core().vkDevice(), _handle.get());

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
    auto const result = core().functions()->vkResetFences(core().vkDevice(), 1, &vkFence);

    CVK_RESULT_STABLE_THROW(result != VK_SUCCESS, result, "failed to reset fence") {}
}

VkResult Fence::wait(uint64_t timeout) const {
    debug_check(*this);

    auto const vkFence = _handle.get();
    auto const result = core().functions()->vkWaitForFences(core().vkDevice(), 1, &vkFence, VK_TRUE, timeout);

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

cth::os::unique_native_handle Fence::extractOsHandle() const {
    debug_check(*this);
    CTH_CRITICAL(
        (_exportTypes & constants::OS_FENCE_HANDLE_TYPE) == 0,
        "fence must be created with constants::OS_FENCE_HANDLE_TYPE in the export types"
    ) {}

#ifdef CTH_PLATFORM_WINDOWS
    // volk loads platform functions only if it was built with the platform define
    auto const getHandle = reinterpret_cast<PFN_vkGetFenceWin32HandleKHR>(
        vkGetDeviceProcAddr(core().vkDevice(), "vkGetFenceWin32HandleKHR")
    );
    CVK_STABLE_THROW(getHandle == nullptr, "vkGetFenceWin32HandleKHR not available") {}

    VkFenceGetWin32HandleInfoKHR const info{
        .sType = VK_STRUCTURE_TYPE_FENCE_GET_WIN32_HANDLE_INFO_KHR,
        .pNext = nullptr,
        .fence = _handle.get(),
        .handleType = constants::OS_FENCE_HANDLE_TYPE,
    };
    HANDLE handle = nullptr;
    auto const result = getHandle(core().vkDevice(), &info, &handle);
#else
    VkFenceGetFdInfoKHR const info{
        .sType = VK_STRUCTURE_TYPE_FENCE_GET_FD_INFO_KHR,
        .pNext = nullptr,
        .fence = _handle.get(),
        .handleType = constants::OS_FENCE_HANDLE_TYPE,
    };
    int handle = cth::os::invalid_handle();
    auto const result = core().functions()->vkGetFenceFdKHR(core().vkDevice(), &info, &handle);
#endif

    CVK_RESULT_STABLE_THROW(result != VK_SUCCESS, result, "failed to export the fence") {}

    return cth::os::unique_native_handle{handle};
}

void Fence::destroy(DeviceTable table, VkFence vkFence) {
    CTH_WARN(vkFence == VK_NULL_HANDLE, "vkFence invalid (VK_NULL_HANDLE)") {}

    table->vkDestroyFence(table.device(), vkFence, nullptr);
}

void Fence::resetState() { _exportTypes = 0; }
}
