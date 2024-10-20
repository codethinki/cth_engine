#pragma once
#include "vulkan/utility/cth_constants.hpp"
#include "vulkan/utility/cth_vk_types.hpp"

#include <cth/pointers.hpp>

#include <volk.h>

namespace cth::vk {
class Core;
class DestructionQueue;


class Fence {
public:
    struct State;

    /**
     * @brief base constructor 
     */
    explicit Fence(cth::not_null<Core const*> core);
    /**
     * @brief constructs and calls @ref wrap()
     * @note calls @ref Fence(cth::not_null<Core const*>)
     */
    explicit Fence(cth::not_null<Core const*> core, State const& state);

    /**
     * @brief constructs and calls @ref create()
     * @note calls @ref Fence(cth::not_null<Core const*>)
     */
    explicit Fence(cth::not_null<Core const*> core, VkFenceCreateFlags flags);


    /**
     * @note calls @ref optDestroy()
     */
    ~Fence() { optDestroy(); };

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
     * @attention requires @ref created()
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
    VkResult wait(uint64_t timeout) const; //NOLINT(modernize-use-nodiscard)

    /**
    * @brief blocks cpu until fence is signaled
    * @attention requires @ref created()
    * @throws cth::vk::result_exception result of @ref vkWaitForFences()
    */
    void wait() const;

    static void destroy(vk::not_null<VkDevice> vk_device, VkFence vk_fence);

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

    static void debug_check(Fence const* fence);
    static void debug_check_handle(VkFence vk_fence);
};
}

//State

namespace cth::vk {
struct Fence::State {
    vk::not_null<VkFence> vkFence;
};
}

//debug checks

namespace cth::vk {
inline void Fence::debug_check(Fence const* fence) {
    CTH_ERR(fence == nullptr, "fence must not be nullptr") throw details->exception();
    debug_check_handle(fence->_handle.get());
}
inline void Fence::debug_check_handle(VkFence vk_fence) {
    CTH_ERR(vk_fence == VK_NULL_HANDLE, "vk_fence handle must not be invalid (VK_NULL_HANDLE)") throw details->exception();
}
}
