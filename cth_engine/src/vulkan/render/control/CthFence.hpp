#pragma once
#include "vulkan/utility/CthConstants.hpp"

#include <cth/cth_memory.hpp>

#include <vulkan/vulkan.h>

namespace cth {
class BasicCore;
class DeletionQueue;


class BasicFence {
public:
    explicit BasicFence(const BasicCore* core);
    virtual ~BasicFence() = default;

    virtual void wrap(VkFence vk_fence);

    /**
     * \throws cth::except::vk_result_exception result of vkCreateFence(...)
     */
    virtual void create(VkFenceCreateFlags flags = 0);
    virtual void destroy(DeletionQueue* deletion_queue = nullptr);


    /**
     * \brief queries the status of the fence
     * \return VkResult of vkGetFenceStatus(...)
     */
    [[nodiscard]] VkResult status() const;
    /**
     * \brief resets the fence
     * \return VkResult of vkResetFences(...)
     */
    [[nodiscard]] VkResult reset() const;
    /**
     * \brief blocks cpu until fence is signaled
     * \param timeout in nanoseconds
     * \return VkResult of vkWaitForFences(...)
     */
    [[nodiscard]] VkResult wait(uint64_t timeout) const;

    static void destroy(VkDevice vk_device, VkFence vk_fence);

protected:
    const BasicCore* _core;

private:
    static VkFenceCreateInfo createInfo(VkFenceCreateFlags flags);

    mem::basic_ptr<VkFence_T> _handle = VK_NULL_HANDLE;

public:
    [[nodiscard]] VkFence get() const { return _handle.get(); }

    BasicFence(const BasicFence& other) = default;
    BasicFence(BasicFence&& other) = default;
    BasicFence& operator=(const BasicFence& other) = default;
    BasicFence& operator=(BasicFence&& other) = default;

#ifdef CONSTANT_DEBUG_MODE
    static void debug_check(const BasicFence* fence);
    static void debug_check_leak(const BasicFence* fence);
#define DEBUG_CHECK_FENCE(fence_ptr) BasicFence::debug_check(fence_ptr)
#define DEBUG_CHECK_FENCE_LEAK(fence_ptr) BasicFence::debug_check_leak(fence_ptr)
#else
#define DEBUG_CHECK_FENCE(fence_ptr) ((void)0)
#define DEBUG_CHECK_FENCE_LEAK(fence_ptr) ((void)0)
#endif
};
}

namespace cth {
class Fence : public BasicFence {
public:
    explicit Fence(const BasicCore* core, DeletionQueue* deletion_queue);
    ~Fence() override;

    void wrap(VkFence vk_fence) override;

    void create(VkFenceCreateFlags flags = 0) override;
    void destroy(DeletionQueue* deletion_queue = nullptr) override;

private:
    DeletionQueue* _deletionQueue = nullptr;

public:
    Fence(const Fence& other) = delete;
    Fence(Fence&& other) = default;
    Fence& operator=(const Fence& other) = delete;
    Fence& operator=(Fence&& other) = default;
};
}
