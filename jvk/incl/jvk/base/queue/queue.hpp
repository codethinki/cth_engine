#pragma once
#include "queue_family.hpp"

#include "jvk/utility/types.hpp"

#include <volk.h>



namespace jvk {
class Device;
struct PresentInfo;
struct SubmitInfo;
class TimelineSemaphore;
class Semaphore;
class Fence;
class Core;
class Swapchain;
class PrimaryCmdBuffer;



class Queue {
public:
    struct State;


    explicit Queue(QueueFamilyProperties family_properties) : _familyProperties{family_properties} {}
    ~Queue();

    /**
     * @brief wraps the vulkan queue
     * @note normally called by the device, not the user
     */
    void wrap(State const& state);

    /**
     * @brief destroys and resets
     * @attention @ref created() required
     */
    void destroy();

    /**
     * @brief if @ref created() calls @ref destroy()
     */
    void optDestroy() { if(created()) destroy(); }

    /**
     * @brief releases ownership and resets
     * @attention requires @ref created()
     */
    State release();

    /**
     * @brief advances and submits the submit_info
     * @note calls @ref const_submit(SubmitInfo const&)
     * @param[in, out] submit_info @attention calls @ref submit_info.next()
     */
    void submit(SubmitInfo& submit_info) const;

    /**
     * @brief submits the submit_info
     * @attention requires @ref created()
     * @note does not advance the @ref submit_info
     */
    void const_submit(SubmitInfo const& submit_info) const;

    /**
     * @brief skip submits without the command_buffer to advance the sync primitives
     * @param[in, out] submit_info @attention calls @ref submit_info.next()
     * @throws jvk::result_exception result of @ref vkQueueSubmit()
     */
    void skip(SubmitInfo& submit_info) const;

    /**
     * @brief does an empty submit without the commandBuffer
     * @note this call respects sync primitives
     * @note does not advance the @ref submit_info
     */
    void const_skip(SubmitInfo const& submit_info) const;

    /**
     * @brief presents the image via vkQueuePresentKHR()
     * @param image_index swapchain image index
     * @return result of @ref vkQueuePresentKHR() [VK_SUCCESS, VK_SUBOPTIMAL_KHR, VK_ERROR_OUT_OF_DATE_KHR]
     * @throws jvk::result_exception result of @ref vkQueuePresentKHR()
     */
    [[nodiscard]] VkResult present(uint32_t image_index, PresentInfo& present_info) const;


    /**
     * presents via vkQueuePresentKHR()
     * @return result of @ref vkQueuePresentKHR() [VK_SUCCESS, VK_SUBOPTIMAL_KHR, VK_ERROR_OUT_OF_DATE_KHR]
     * @throws jvk::result_exception result of @ref vkQueuePresentKHR()
     */
    [[nodiscard]] VkResult raw_present(VkPresentInfoKHR const& present_info) const;

    /**
     * @brief skips presenting the info
     * @note this call respects sync primitives
     */
    void const_skip(PresentInfo const& present_info) const;

    /**
     * Submits the raw VkSubmitInfo to the queue
     * @throws jvk::vk_result_exception if vkQueueSubmit != VK_SUCCESS
     */
    void raw_submit(VkSubmitInfo const& submit_info, VkFence fence = nullptr) const;

    /**
     * Blocks cpu until all queue gpu operations are finished
     * @details calls vkQueueWaitIdle()
     */
    void wait() const;

private:
    void reset();



    QueueFamilyProperties _familyProperties;

    move_ptr<VkQueue_T> _handle = VK_NULL_HANDLE;
    Device const* _device = nullptr;
    uint32_t _familyIndex = 0;
    uint32_t _queueIndex = 0;

public:
    [[nodiscard]] bool can_present() const { return contains(_familyProperties, QueueFamilyProperties::PRESENT); }
    [[nodiscard]] bool created() const { return _handle != VK_NULL_HANDLE; }
    [[nodiscard]] auto get() const { return _handle.get(); }
    [[nodiscard]] auto index() const { return _queueIndex; }

    [[nodiscard]] auto familyIndex() const { return _familyIndex; }
    [[nodiscard]] auto familyProperties() const { return _familyProperties; }

    Queue(Queue const& other) = default;
    Queue(Queue&& other) = default;
    Queue& operator=(Queue const& other) = default;
    Queue& operator=(Queue&& other) = default;

    static void debug_check(Queue const& queue);
    static void debug_check_present(Queue const& queue);
    static void debug_check_handle(VkQueue vk_queue);
};
}

//State

namespace jvk {
struct Queue::State {
    jvk::vk_not_null<VkQueue> vkQueue;
    not_null<Device const*> device;
    uint32_t familyIndex;
    /**
     * @brief index in the family
     */
    uint32_t queueIndex;
};
}

//debug checks

namespace jvk {
inline void Queue::debug_check(Queue const& queue) {
    CTH_CRITICAL(!queue.created(), "queue must be created") {}
}

inline void Queue::debug_check_present(Queue const& queue) {
    debug_check(queue);
    CTH_CRITICAL(
        !queue.can_present(),
        "queue is not a present queue"
    ) {}
}

inline void Queue::debug_check_handle(VkQueue vk_queue) {
    CTH_CRITICAL(vk_queue == VK_NULL_HANDLE, "vk_queue handle must not be invalid (VK_NULL_HANDLE)") {}
}


}
