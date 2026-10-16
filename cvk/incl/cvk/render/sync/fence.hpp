#pragma once
#include "cvk/base/device_table.hpp"
#include "cvk/utility/types.hpp"

#include <volk.h>

#include <cth/io/log.hpp>
#include <cth/utility.hpp>

#include <cstdint>


namespace cvk {
class Core;

/**
 * @brief VkFence wrapper
 */
class Fence {
public:
    struct State;

    /**
     * @brief base constructor
     * @pre @ref Core::created()
     */
    explicit Fence(Core const& core);

    /**
     * @brief constructs and wraps
     * @details calls:
     * - @ref Fence(Core const&)
     * - @ref wrap()
     */
    Fence(Core const& core, State const& state);

    /**
     * @brief constructs and creates
     * @details calls:
     * - @ref Fence(Core const&)
     * - @ref create()
     */
    Fence(Core const& core, VkFenceCreateFlags flags);

    /**
     * @note calls @ref optDestroy()
     */
    ~Fence() { optDestroy(); }

    /**
     * @brief wraps the state
     * @post @ref created()
     * @note calls @ref optDestroy()
     */
    void wrap(State const& state);

    /**
     * @brief creates the fence
     * @param flags VK_FENCE_CREATE_SIGNALED_BIT -> created signaled
     * @post @ref created()
     * @note calls @ref optDestroy()
     * @throws cvk::vk_result_exception result of vkCreateFence()
     */
    void create(VkFenceCreateFlags flags = 0);

    /**
     * @brief destroys and resets
     * @pre @ref created()
     * @pre the gpu does not use the fence
     */
    void destroy();

    /**
     * @brief if @ref created() -> calls @ref destroy()
     */
    void optDestroy() { if(created()) destroy(); }

    /**
     * @brief releases ownership, returns the state and resets
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

    static void destroy(DeviceTable table, VkFence vkFence);

    [[nodiscard]] bool created() const { return _handle != VK_NULL_HANDLE; }
    [[nodiscard]] VkFence get() const { return _handle.get(); }
    [[nodiscard]] DeviceTable deviceTable() const { return _deviceTable; }

private:
    void resetState();

    [[nodiscard]] static VkFenceCreateInfo createInfo(VkFenceCreateFlags flags);

    DeviceTable _deviceTable;
    move_ptr<VkFence_T> _handle = VK_NULL_HANDLE;

public:
    Fence(Fence const& other) = delete;
    Fence(Fence&& other) noexcept = default;
    Fence& operator=(Fence const& other) = delete;
    Fence& operator=(Fence&& other) noexcept { return cth::reconstruct_at(*this, std::move(other)); }

    static void debug_check(Fence const& fence);
};
}

namespace cvk {
struct Fence::State {
    vk_not_null<VkFence> vkFence;
};
}

namespace cvk {
inline void Fence::debug_check(Fence const& fence) {
    CTH_CRITICAL(!fence.created(), "fence must be created") {}
}
}
