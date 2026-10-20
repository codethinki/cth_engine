#pragma once
#include "queue_family.hpp"
#include "queue_state.hpp"
#include "submit_info.hpp"

#include <volk.h>

#include <cth/io/log.hpp>

#include <span>


namespace cvk {
class Fence;

/**
 * @brief VkQueue of a device
 * @details copyable, copies share the vulkan queue,
 *  the device owns the vulkan queue, a queue has no lifecycle
 * @attention not thread safe: one thread at a time per vulkan queue, Vulkan requires it
 */
class Queue {
public:
    using vk_handle_t = VkQueue;
    using State = QueueState;

    /**
     * @brief the device constructs its queues
     */
    Queue(QueueFamilyProperties familyProperties, State const&);

    /**
     * @brief submits the infos in one vkQueueSubmit2()
     * @param fence nullptr -> no fence
     * @details resolves the std::nullopt timeline values in order: the waits, then the signals of each info
     * @pre @ref created()
     * @pre the command buffers ended their recording
     * @pre the semaphores and the fence are created, the fence is unsignaled
     * @throws cvk::vk_result_exception result of vkQueueSubmit2()
     */
    void submit(std::span<SubmitInfo const> infos, Fence const* fence = nullptr) const;

    /**
     * @brief submits the info
     * @details calls @ref submit(std::span<SubmitInfo const>, Fence const*) const
     */
    void submit(SubmitInfo const& info, Fence const* fence = nullptr) const { submit({&info, 1}, fence); }

    /**
     * @brief submits to the vulkan queue, for what @ref SubmitInfo does not cover
     * @param fence VK_NULL_HANDLE -> no fence
     * @throws cvk::vk_result_exception result of vkQueueSubmit2()
     */
    void rawSubmit(VkSubmitInfo2 const& submitInfo, VkFence fence = VK_NULL_HANDLE) const;

    /**
     * @brief presents through vkQueuePresentKHR()
     * @return VK_SUCCESS, VK_SUBOPTIMAL_KHR or VK_ERROR_OUT_OF_DATE_KHR
     * @pre @ref canPresent()
     * @throws cvk::vk_result_exception other result of vkQueuePresentKHR()
     */
    [[nodiscard]] VkResult rawPresent(VkPresentInfoKHR const& presentInfo) const;

    /**
     * @brief blocks until the queue finished all its work
     * @throws cvk::vk_result_exception result of vkQueueWaitIdle()
     */
    void waitIdle() const;

    [[nodiscard]] constexpr bool canPresent() const {
        return contains(_familyProperties, QueueFamilyProperties::PRESENT);
    }
    [[nodiscard]] constexpr vk_handle_t get() const { return _handle.get(); }
    [[nodiscard]] constexpr uint32_t index() const { return _queueIndex; }
    [[nodiscard]] constexpr uint32_t familyIndex() const { return _familyIndex; }
    [[nodiscard]] constexpr QueueFamilyProperties familyProperties() const { return _familyProperties; }

private:
    QueueFamilyProperties _familyProperties;
    VolkDeviceTable const* _functions = nullptr;

    move_ptr<VkQueue_T> _handle = VK_NULL_HANDLE;

    uint32_t _familyIndex = 0;
    uint32_t _queueIndex = 0;

public:
    Queue(Queue const& other) = default;
    Queue(Queue&& other) = default;
    Queue& operator=(Queue const& other) = default;
    Queue& operator=(Queue&& other) = default;

    static constexpr void debug_check(Queue const&);
    static constexpr void debug_check_present(Queue const&);
};
}

namespace cvk {
constexpr void Queue::debug_check(Queue const& queue) {
    CTH_CRITICAL(queue.get() == VK_NULL_HANDLE, "queue must not be moved from") {}
}

constexpr void Queue::debug_check_present(Queue const& queue) {
    debug_check(queue);
    CTH_CRITICAL(!queue.canPresent(), "queue is not a present queue") {}
}
}
