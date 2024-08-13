#pragma once
#include "vulkan/utility/cth_constants.hpp"

#include<cth/cth_pointer.hpp>

#include <vulkan/vulkan.h>

namespace cth::vk {
class BasicCore;
class DestructionQueue;


class BasicFence {
public:
    explicit BasicFence(BasicCore const* core);
    virtual ~BasicFence() = default;

    virtual void wrap(VkFence vk_fence);

    /**
     * @throws cth::except::vk_result_exception result of vkCreateFence()
     */
    virtual void create(VkFenceCreateFlags flags = 0);
    virtual void destroy(DestructionQueue* destruction_queue = nullptr);


    /**
     * @brief queries the status of the fence
     * @return VkResult of vkGetFenceStatus() [VK_SUCCESS, VK_NOT_READY]
     * @throws cth::except::vk_result_exception result of vkGetFenceStatus()
     */
    [[nodiscard]] VkResult status() const;
    /**
     * @brief resets the fence
     * @throws cth::except::vk_result_exception result of vkResetFences()
     */
    void reset() const;

    /**
     * @brief blocks cpu until fence is signaled or the timeout is reached
     * @param timeout in nanoseconds
     * @return VkResult of vkWaitForFences() [VK_SUCCESS, VK_TIMEOUT]
     * @throws cth::except::vk_result_exception result of vkWaitForFences()
     */
    VkResult wait(uint64_t timeout) const; //NOLINT(modernize-use-nodiscard)

    /**
    * @brief blocks cpu until fence is signaled
    * @throws cth::except::vk_result_exception result of vkWaitForFences()
    */
    void wait() const;

    static void destroy(VkDevice vk_device, VkFence vk_fence);

protected:
    BasicCore const* _core;

private:
    static [[nodiscard]] VkResult wait(VkDevice vk_device, std::span<VkFence const> vk_fences, uint64_t timeout);
    static [[nodiscard]] void wait(VkDevice vk_device, std::span<VkFence const> vk_fences);

    static VkFenceCreateInfo createInfo(VkFenceCreateFlags flags);

    move_ptr<VkFence_T> _handle = VK_NULL_HANDLE;

public:
    [[nodiscard]] VkFence get() const { return _handle.get(); }

    BasicFence(BasicFence const& other) = default;
    BasicFence(BasicFence&& other) = default;
    BasicFence& operator=(BasicFence const& other) = default;
    BasicFence& operator=(BasicFence&& other) = default;

#ifdef CONSTANT_DEBUG_MODE
    static void debug_check(BasicFence const* fence);
    static void debug_check_leak(BasicFence const* fence);
    static void debug_check_handle(VkFence vk_fence);
    template<class Rng>
    static void debug_check_handles(Rng const& rng) { for(auto const& fence : rng) BasicFence::debug_check_handle(fence); }
#define DEBUG_CHECK_FENCE(fence_ptr) BasicFence::debug_check(fence_ptr)
#define DEBUG_CHECK_FENCE_HANDLE(vk_fence) BasicFence::debug_check_handle(vk_fence)
#define DEBUG_CHECK_FENCE_HANDLES(vk_fences) BasicFence::debug_check_handles(vk_fences)
#define DEBUG_CHECK_FENCE_LEAK(fence_ptr) BasicFence::debug_check_leak(fence_ptr)
#else
#define DEBUG_CHECK_FENCE(fence_ptr) ((void)0)
#define DEBUG_CHECK_FENCE_HANDLE(vk_fence) ((void)0)
#define DEBUG_CHECK_FENCE_LEAK(fence_ptr) ((void)0)
#endif
};
}

namespace cth::vk {
class Fence : public BasicFence {
public:
    explicit Fence(BasicCore const* core, DestructionQueue* destruction_queue, VkFenceCreateFlags flags = 0);
    ~Fence() override;

    void wrap(VkFence vk_fence) override;

    void create(VkFenceCreateFlags flags = 0) override;
    void destroy(DestructionQueue* destruction_queue = nullptr) override;

private:
    DestructionQueue* _destructionQueue = nullptr;

public:
    Fence(Fence const& other) = delete;
    Fence(Fence&& other) = default;
    Fence& operator=(Fence const& other) = delete;
    Fence& operator=(Fence&& other) = default;
};
}
