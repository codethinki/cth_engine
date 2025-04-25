module;
#include "lib/volk.hpp"
#include <cth/io/io_log.hpp>

export module cth.vk.render.sync.fence;

import cth.vk.base.device_table;
import cth.vk.base.core;
import cth_vk_res_destruction_queue;
import cth.vk.constants;
import cth.vk.util.types;

import cth.ptr;
import cth.io.log;


export namespace cth::vk {
class Fence {
public:
    using wait_t = uint64_t;

    struct State;
    /**
     * @brief base constructor 
     */
    explicit Fence(Core const& core);
    /**
     * @brief constructs and calls @ref wrap()
     * @note calls @ref Fence(Core const&)
     */
    explicit Fence(Core const& core, State const& state);

    /**
     * @brief constructs and calls @ref create()
     * @note calls @ref Fence(Core const&)
     */
    explicit Fence(Core const& core, VkFenceCreateFlags flags);


    /**
     * @note calls @ref optDestroy()
     */
    ~Fence() { optDestroy(); }

    /**
     * @brief wraps @ref State
     * @note calls @ref optDestroy()
     */
    void wrap(State const& state);

    /**
     * @brief creates the fence
     * @note calls @ref optDestroy()
     * @throws cth::vk::result_exception result of @ref vkCreateFence()
     */
    void create(VkFenceCreateFlags flags = 0);

    /**
     * @brief destroys and resets the object
     * @note pushes to @ref Core::destructionQueue() if available
     * @note calls @ref Fence::destroy(vk::not_null<VkDevice>, VkFence)
     */
    void destroy();

    /**
     * @brief if @ref created() calls @ref destroy()
     */
    void optDestroy() { if(created()) destroy(); }

    /**
     * @brief queries the status of the fence
     * @attention requires @ref created() const
     * @return VkResult of vkGetFenceStatus() [VK_SUCCESS, VK_NOT_READY]
     * @throws cth::vk::result_exception result of @ref vkGetFenceStatus()
     */
    [[nodiscard]] VkResult status() const;

    /**
     * @brief resets the fence
     * @attention requires @ref created()
     * @throws cth::vk::result_exception result of @ref vkResetFences()
     */
    void reset() const;

    /**
     * @brief blocks cpu until fence is signaled or the timeout is reached
     * @attention requires @ref created()
     * @param timeout in nanoseconds
     * @return VkResult of vkWaitForFences() [VK_SUCCESS, VK_TIMEOUT]
     * @throws cth::vk::result_exception result of @ref vkWaitForFences()
     */
    [[nodiscard]] VkResult wait(wait_t timeout) const;

    /**
     * @brief waits and resets
     * @details calls:
            - @ref wait() const
            - @ref reset() const
     */
    void waitReset() const;


    /**
     * @brief waits [0, timeout) and resets
     * @param timeout in nanoseconds
     * @return VkResult of @ref wait(wait_t) const
     * @details calls:
            - @ref wait(wait_t) const
            - @ref reset() const
     */
    [[nodiscard]] VkResult waitReset(wait_t timeout) const;


    /**
    * @brief blocks cpu until fence is signaled
    * @attention requires @ref created()
    * @throws cth::vk::result_exception result of @ref vkWaitForFences()
    */
    void wait() const;

    static void destroy(DeviceTable table, VkFence vk_fence);

private:
    void resetState();

    static VkFenceCreateInfo createInfo(VkFenceCreateFlags flags);

    cth::not_null<Core const*> _core;

    move_ptr<VkFence_T> _handle = VK_NULL_HANDLE;

public:
    [[nodiscard]] VkFence get() const { return _handle.get(); }
    [[nodiscard]] bool created() const { return _handle != VK_NULL_HANDLE; }

    Fence(Fence const& other) = default;
    Fence(Fence&& other) = default;
    Fence& operator=(Fence const& other) = default;
    Fence& operator=(Fence&& other) = default;

    static void debug_check(Fence const& fence);
    static void debug_check_handle(vk::not_null<VkFence> vk_fence);
};
}

//State

export namespace cth::vk {
struct Fence::State {
    vk::not_null<VkFence> vkFence;
};
}

//debug checks

export namespace cth::vk {
void Fence::debug_check(Fence const& fence) { debug_check_handle(fence._handle.get()); }
void Fence::debug_check_handle([[maybe_unused]] vk::not_null<VkFence> vk_fence) {}
}
