#pragma once
#include "cvk/base/device_table.hpp"
#include "cvk/utility/constants.hpp"
#include "cvk/utility/handle/core_handle.hpp"
#include "cvk/utility/handle/destroy.hpp"
#include "cvk/utility/types.hpp"

#include <volk.h>

#include <cth/io/log.hpp>
#include <cth/os/native_handle.hpp>

#include <cstdint>
#include <tuple>


namespace cvk {
class Core;

/**
 * VkFence wrapper
 */
class Fence {
public:
    struct State;

    /**
     * base constructor
     * @pre @ref Core::created()
     */
    explicit Fence(Core const&);

    /**
     * constructs and wraps
     * @details calls @ref Fence(Core const&), @ref wrap()
     */
    Fence(Core const&, State const&);

    /**
     * constructs and creates
     * @details calls @ref Fence(Core const&), @ref create()
     */
    explicit Fence(Core const&, create_t, VkFenceCreateFlags flags = 0, VkExternalFenceHandleTypeFlags exportTypes = 0);

    /**
     * @note calls @ref optDestroy()
     */
    ~Fence() { optDestroy(); }

    /**
     * wraps the state
     * @post @ref created()
     * @note calls @ref optDestroy()
     */
    void wrap(State const&);

    /**
     * creates the fence
     * @param flags VK_FENCE_CREATE_SIGNALED_BIT -> created signaled
     * @param exportTypes handle types the fence can export, 0 -> not exportable,
     *  @ref constants::OS_FENCE_HANDLE_TYPE for @ref extractOsHandle()
     * @post @ref created()
     * @note calls @ref optDestroy()
     * @throws cvk::vk_result_exception result of vkCreateFence()
     */
    void create(VkFenceCreateFlags flags = 0, VkExternalFenceHandleTypeFlags exportTypes = 0);

    /**
     * destroys and forgets
     * @pre @ref created()
     * @pre the gpu does not use the fence
     */
    void destroy();

    /**
     * if @ref created() -> calls @ref destroy()
     */
    void optDestroy() { if(created()) destroy(); }

    /**
     * releases ownership, returns the state and forgets
     * @pre @ref created()
     */
    State release();

    /**
     * @return VK_SUCCESS -> signaled, VK_NOT_READY -> not signaled
     * @pre @ref created()
     * @throws cvk::vk_result_exception other result of vkGetFenceStatus()
     */
    [[nodiscard]] VkResult status() const;

    /**
     * @pre @ref created()
     */
    [[nodiscard]] bool signaled() const { return status() == VK_SUCCESS; }

    /**
     * sets the fence to not signaled
     * @pre @ref created()
     * @throws cvk::vk_result_exception result of vkResetFences()
     */
    void reset() const;

    /**
     * blocks until the fence is signaled or the timeout ends
     * @param timeout in nanoseconds
     * @return VK_SUCCESS or VK_TIMEOUT
     * @pre @ref created()
     * @throws cvk::vk_result_exception other result of vkWaitForFences()
     */
    [[nodiscard]] VkResult wait(uint64_t timeout) const;

    /**
     * blocks until the fence is signaled
     * @pre @ref created()
     * @throws cvk::vk_result_exception result of vkWaitForFences()
     */
    void wait() const;

    /**
     * waits, then resets if signaled
     * @param timeout in nanoseconds
     * @return VK_SUCCESS -> reset, VK_TIMEOUT -> not reset
     */
    [[nodiscard]] VkResult waitReset(uint64_t timeout) const;

    /**
     * waits and resets
     */
    void waitReset() const;

    /**
     * exports the fence as an OS handle, see @ref constants::OS_FENCE_HANDLE_TYPE
     * @details the fence keeps its state
     * @details posix: invalid handle -> already signaled
     * @return handle, waitable with cth::os::wait() or a scheduler
     * @pre @ref created() with @ref constants::OS_FENCE_HANDLE_TYPE in the export types
     * @pre the device enables @ref constants::OS_FENCE_DEVICE_EXTENSIONS
     * @pre posix: the device can import @ref constants::OS_FENCE_HANDLE_TYPE
     * @pre posix: the fence is signaled or a signal operation is pending
     * @attention posix: blocks until the pending work reaches the kernel, work that waits for the host -> deadlock
     * @throws cvk::vk_result_exception result of vkGetFenceWin32HandleKHR(), vkGetFenceFdKHR() or vkImportFenceFdKHR()
     * @throws cvk::cvk_exception reason: posix: the duplication of the handle failed
     */
    [[nodiscard]] cth::os::unique_native_handle extractOsHandle() const;

    [[nodiscard]] constexpr bool created() const { return static_cast<bool>(_handle); }
    [[nodiscard]] constexpr VkFence get() const { return _handle.get(); }
    [[nodiscard]] constexpr Core const& core() const { return _handle.core(); }
    [[nodiscard]] constexpr VkExternalFenceHandleTypeFlags exportTypes() const { return _exportTypes; }

private:
    void forget();

    core_handle<VkFence> _handle;
    VkExternalFenceHandleTypeFlags _exportTypes = 0;

public:
    Fence(Fence const& other) = delete;
    Fence(Fence&& other) noexcept = default;
    Fence& operator=(Fence const& other) = delete;
    Fence& operator=(Fence&& other) noexcept = default;

    static constexpr void debug_check(Fence const&);
};
}

namespace cvk {
struct Fence::State {
    vk_not_null<VkFence> vkFence;
    /**
     * handle types the fence was created to export
     */
    VkExternalFenceHandleTypeFlags exportTypes = 0;

    /**
     * the raw handles to destroy, in destruction order
     */
    [[nodiscard]] constexpr std::tuple<VkFence> decompose() && { return {vkFence.get()}; }
};
}

namespace cvk {
constexpr void Fence::debug_check(Fence const& fence) {
    CTH_CRITICAL(!fence.created(), "fence must be created") {}
}
}
