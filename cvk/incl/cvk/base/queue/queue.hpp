#pragma once
#include "queue_family.hpp"
#include "queue_state.hpp"

#include <volk.h>

#include <cth/io/log.hpp>


namespace cvk {
/**
 * @brief VkQueue wrapper
 * @details copyable, thread safe, copies share the mutex of the vulkan queue
 */
class Queue {
public:
    using vk_handle_t = VkQueue;
    using State = QueueState;

    explicit Queue(QueueFamilyProperties familyProperties) : _familyProperties{familyProperties} {}
    Queue(QueueFamilyProperties familyProperties, State state) : Queue{familyProperties} { wrap(std::move(state)); }
    ~Queue() { optDestroy(); }

    /**
     * @brief wraps the vulkan queue
     * @details the device calls this, not the user
     * @post @ref created()
     * @note calls @ref optDestroy()
     */
    void wrap(State);

    /**
     * @brief forgets, the device owns the vulkan queue
     * @pre @ref created()
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
     * @brief submits to the vulkan queue
     * @param fence VK_NULL_HANDLE -> no fence
     * @pre @ref created()
     * @throws cvk::vk_result_exception result of vkQueueSubmit2()
     */
    void raw_submit(VkSubmitInfo2 const& submitInfo, VkFence fence = VK_NULL_HANDLE) const;

    /**
     * @brief presents through vkQueuePresentKHR()
     * @return VK_SUCCESS, VK_SUBOPTIMAL_KHR or VK_ERROR_OUT_OF_DATE_KHR
     * @pre @ref created()
     * @pre @ref can_present()
     * @throws cvk::vk_result_exception other result of vkQueuePresentKHR()
     */
    [[nodiscard]] VkResult raw_present(VkPresentInfoKHR const& presentInfo) const;

    /**
     * @brief blocks until the queue finished all its work
     * @pre @ref created()
     * @throws cvk::vk_result_exception result of vkQueueWaitIdle()
     */
    void wait() const;

    [[nodiscard]] constexpr bool can_present() const {
        return contains(_familyProperties, QueueFamilyProperties::PRESENT);
    }
    [[nodiscard]] constexpr bool created() const { return _handle != VK_NULL_HANDLE; }
    [[nodiscard]] constexpr vk_handle_t get() const { return _handle.get(); }
    [[nodiscard]] constexpr uint32_t index() const { return _queueIndex; }
    [[nodiscard]] constexpr uint32_t familyIndex() const { return _familyIndex; }
    [[nodiscard]] constexpr QueueFamilyProperties familyProperties() const { return _familyProperties; }

private:
    void forget();

    QueueFamilyProperties _familyProperties;
    VolkDeviceTable const* _functions = nullptr;

    move_ptr<VkQueue_T> _handle = VK_NULL_HANDLE;
    std::shared_ptr<std::mutex> _handleMtx{};

    uint32_t _familyIndex = 0;
    uint32_t _queueIndex = 0;

public:
    Queue(Queue const& other) = default;
    Queue(Queue&& other) = default;
    Queue& operator=(Queue const& other) = default;
    Queue& operator=(Queue&& other) = default;

    static constexpr void debug_check(Queue const&);
    static constexpr void debug_check_present(Queue const&);
    static constexpr void debug_check_handle(VkQueue);
};
}

namespace cvk {
constexpr void Queue::debug_check(Queue const& queue) {
    CTH_CRITICAL(!queue.created(), "queue must be created") {}
}

constexpr void Queue::debug_check_present(Queue const& queue) {
    debug_check(queue);
    CTH_CRITICAL(!queue.can_present(), "queue is not a present queue") {}
}

constexpr void Queue::debug_check_handle(VkQueue vkQueue) {
    CTH_CRITICAL(vkQueue == VK_NULL_HANDLE, "vkQueue must not be invalid (VK_NULL_HANDLE)") {}
}
}
