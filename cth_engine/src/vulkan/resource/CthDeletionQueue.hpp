#pragma once
#include "vulkan/utility/CthConstants.hpp"

#include <vulkan/vulkan.h>

#include <array>
#include <queue>
#include <variant>
#include <vector>
#include <cth/cth_type_traits.hpp>

namespace cth {
class BasicCore;

class DeletionQueue {
public:
    using deletable_handle_t = std::variant<
        VkDeviceMemory, VkBuffer, VkImage,
        VkSemaphore, VkFence,
        VkCommandPool,
        VkSwapchainKHR
    >;
    using dependent_handle_t = std::variant<
        VkCommandBuffer>;

    explicit DeletionQueue(BasicCore* core);
    ~DeletionQueue();

    void push(deletable_handle_t handle);
    void push(dependent_handle_t handle, deletable_handle_t dependency);


    void clear(uint32_t current_frame);
    void next(const uint32_t next_frame) { _frame = next_frame; }

private:
    using handle_t = std::variant<
        deletable_handle_t,
        dependent_handle_t
    >;
    struct deletable {
        handle_t handle;
        deletable_handle_t dependency;
    };

    static constexpr size_t QUEUES = Constant::FRAMES_IN_FLIGHT;

    uint32_t _frame = 0;
    BasicCore* _core; //temp make this const_ptr

    std::array<std::vector<deletable>, QUEUES> _queue;

public:
    [[nodiscard]] uint32_t currentFrame() const { return _frame; }

    DeletionQueue(const DeletionQueue& other) = delete;
    DeletionQueue(DeletionQueue&& other) = default;
    DeletionQueue& operator=(const DeletionQueue& other) = delete;
    DeletionQueue& operator=(DeletionQueue&& other) = default;

#ifdef CONSTANT_DEBUG_MODE
    static void debug_check(const DeletionQueue* queue);
    static void debug_check_null_allowed(const DeletionQueue* queue);
#define DEBUG_CHECK_DELETION_QUEUE(deletion_queue_ptr) DeletionQueue::debug_check(deletion_queue_ptr)
#define DEBUG_CHECK_DELETION_QUEUE_NULL_ALLOWED(deletion_queue_ptr) 
#else
#define DEBUG_CHECK_DELETION_QUEUE(deletion_queue_ptr) ((void)0)
#define DEBUG_CHECK_DELETION_QUEUE_NULL_ALLOWED(deletion_queue_ptr) ((void)0) 
#endif
};

}
