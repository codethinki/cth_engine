#pragma once
#include "cvk/base/device_table.hpp"
#include "cvk/utility/handle/core_handle.hpp"
#include "cvk/utility/types.hpp"

#include <volk.h>

#include <cth/io/log.hpp>
#include <cth/os/native_handle.hpp>

#include <cstdint>


namespace cvk {
class Core;

/**
 * @brief VkFence wrapper
 */
class Fence {
public:
    using vk_handle_t = VkFence;
    struct State;

    /**
     * @brief base constructor
     * @pre @ref Core::created()
     */
    explicit Fence(Core const&);

    /**
     * @brief constructs and wraps
     * @details calls:
     * - @ref Fence(Core const&)
     * - @ref wrap()
     */
    Fence(Core const&, State const&);

    /**
     * @brief constructs and creates
     * @details calls:
     * - @ref Fence(Core const&)
     * - @ref create()
     */
    Fence(Core const&, VkFenceCreateFlags flags, VkExternalFenceHandleTypeFlags exportTypes = 0);

    /**
     * @note calls @ref optDestroy()
     */
    ~Fence() { optDestroy(); }

    /**
     * @brief wraps the state
     * @post @ref created()
     * @note calls @ref optDestroy()
     */
    void wrap(State const&);

    /**
     * @brief creates the fence
     * @param flags VK_FENCE_CREATE_SIGNALED_BIT -> created signaled
     * @param exportTypes handle types the fence can export, 0 -> not exportable,
     *  @ref constants::OS_FENCE_HANDLE_TYPE for @ref extractOsHandle()
     * @post @ref created()
     * @note calls @ref optDestroy()
     * @throws cvk::vk_result_exception result of vkCreateFence()
     */
    void create(VkFenceCreateFlags flags = 0, VkExternalFenceHandleTypeFlags exportTypes = 0);

    /**
     * @brief destroys and forgets
     * @pre @ref created()
     * @pre the gpu does not use the fence
     */
    void destroy();

    /**
     * @brief if @ref created() -> calls @ref destroy()
     */
    void optDestroy() { if(created()) destroy(); }

    /**
     * @brief releases ownership, returns the state and forgets
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
     * @brief sets the fence to not signaled
     * @pre @ref created()
     * @throws cvk::vk_result_exception result of vkResetFences()
     */
    void reset() const;

    /**
     * @brief blocks until the fence is signaled or the timeout ends
     * @param timeout in nanoseconds
     * @return VK_SUCCESS or VK_TIMEOUT
     * @pre @ref created()
     * @throws cvk::vk_result_exception other result of vkWaitForFences()
     */
    [[nodiscard]] VkResult wait(uint64_t timeout) const;

    /**
     * @brief blocks until the fence is signaled
     * @pre @ref created()
     * @throws cvk::vk_result_exception result of vkWaitForFences()
     */
    void wait() const;

    /**
     * @brief waits, then resets if signaled
     * @param timeout in nanoseconds
     * @return VK_SUCCESS -> reset, VK_TIMEOUT -> not reset
     * @details calls:
     * - @ref wait(uint64_t) const
     * - @ref reset()
     */
    [[nodiscard]] VkResult waitReset(uint64_t timeout) const;

    /**
     * @brief waits and resets
     * @details calls:
     * - @ref wait() const
     * - @ref reset()
     */
    void waitReset() const;

    /**
     * @brief exports the fence as an OS handle, see @ref constants::OS_FENCE_HANDLE_TYPE
     * @details windows: the fence keeps its state,
     *  posix: the export resets the fence (sync fd, copy transference), invalid handle -> already signaled
     * @return handle, waitable with cth::os::wait() or a scheduler
     * @pre @ref created() with @ref constants::OS_FENCE_HANDLE_TYPE in the export types
     * @pre the device enables @ref constants::OS_FENCE_DEVICE_EXTENSIONS
     * @pre posix: the fence is signaled or a signal operation is pending
     * @attention posix: blocks until the pending work reaches the kernel, e.g. a wait before signal on a timeline
     *  semaphore that the host signals later -> deadlock
     * @throws cvk::vk_result_exception result of vkGetFenceWin32HandleKHR() or vkGetFenceFdKHR()
     */
    [[nodiscard]] cth::os::unique_native_handle extractOsHandle() const;

    static void destroy(DeviceTable table, VkFence);

    [[nodiscard]] constexpr bool created() const { return static_cast<bool>(_handle); }
    [[nodiscard]] constexpr vk_handle_t get() const { return _handle.get(); }
    [[nodiscard]] constexpr Core const& core() const { return _handle.core(); }
    [[nodiscard]] constexpr VkExternalFenceHandleTypeFlags exportTypes() const { return _exportTypes; }

private:
    void forget();

    core_handle<Fence> _handle;
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
     * @brief handle types the fence was created to export
     */
    VkExternalFenceHandleTypeFlags exportTypes = 0;
};
}

namespace cvk {
constexpr void Fence::debug_check(Fence const& fence) {
    CTH_CRITICAL(!fence.created(), "fence must be created") {}
}
}
