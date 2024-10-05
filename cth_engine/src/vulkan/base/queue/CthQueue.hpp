#pragma once
#include "CthQueueFamily.hpp"

#include "vulkan/utility/cth_constants.hpp"
#include "vulkan/utility/cth_vk_types.hpp"

#include <cth/pointers.hpp>

#include <vulkan/vulkan.h>



namespace cth::vk {
struct PresentInfo;
struct SubmitInfo;
class TimelineSemaphore;
class Semaphore;
class Fence;
class Core;
class BasicSwapchain;
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
     * @throws cth::vk::result_exception result of @ref vkQueueSubmit()
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
     * @throws cth::vk::result_exception result of @ref vkQueuePresentKHR()
     */
    [[nodiscard]] VkResult present(uint32_t image_index, PresentInfo& present_info) const;

    /**
     * @brief skips presenting the info
     * @note this call respects sync primitives
     */
    void const_skip(PresentInfo const& present_info) const;

private:
    void reset();

    void submit(VkSubmitInfo const* submit_info, VkFence fence = nullptr) const;


    QueueFamilyProperties _familyProperties;

    cth::move_ptr<VkQueue_T> _handle = VK_NULL_HANDLE;
    uint32_t _familyIndex = 0;
    uint32_t _queueIndex = 0;

public:
    [[nodiscard]] auto created() const { return _handle != VK_NULL_HANDLE; }
    [[nodiscard]] auto get() const { return _handle.get(); }
    [[nodiscard]] auto index() const { return _queueIndex; }

    [[nodiscard]] auto familyIndex() const { return _familyIndex; }
    [[nodiscard]] auto familyProperties() const { return _familyProperties; }

    Queue(Queue const& other) = default;
    Queue(Queue&& other) = default;
    Queue& operator=(Queue const& other) = default;
    Queue& operator=(Queue&& other) = default;

    static void debug_check(cth::not_null<Queue const*> queue);
    static void debug_check_present(cth::not_null<Queue const*> queue);
    static void debug_check_handle(VkQueue vk_queue);
};
}

//State

namespace cth::vk {
struct Queue::State {
    vk::not_null<VkQueue> vkQueue;
    uint32_t familyIndex;
    /**
     * @brief index in the family
     */
    uint32_t queueIndex;
};
}

//debug checks

namespace cth::vk {
inline void Queue::debug_check(cth::not_null<Queue const*> queue) {
    CTH_CRITICAL(!queue->created(), "queue must be created") {}
}
inline void Queue::debug_check_present(cth::not_null<Queue const*> queue) {
    debug_check(queue);
    CTH_CRITICAL(!(queue->familyProperties() & QUEUE_FAMILY_PROPERTY_PRESENT), "queue is not a present queue") {}
}
inline void Queue::debug_check_handle(VkQueue vk_queue) {
    CTH_CRITICAL(vk_queue == VK_NULL_HANDLE, "vk_queue handle must not be invalid (VK_NULL_HANDLE)") {}
}


}