#pragma once
#include "vulkan/utility/CthConstants.hpp"

#include <vulkan/vulkan.h>

#include <array>
#include <variant>
#include <vector>


namespace cth {
class Context;


class DeletionQueue {
public:
    using deletable_handle_t = std::variant<
        //resource
        VkDeviceMemory, VkBuffer, VkImage,
        //control
        VkSemaphore,
        //debug
        VkDebugUtilsMessengerEXT,
        //base
        VkInstance
    >;

    explicit DeletionQueue(Context* context);
    ~DeletionQueue();

    void push(deletable_handle_t handle);
    void clear(uint32_t current_frame);
    void next(const uint32_t next_frame) { _frame = next_frame; }

private:
    static constexpr size_t QUEUES = Constant::MAX_FRAMES_IN_FLIGHT;
    uint32_t _frame = 0;
    Context* _context;
    std::array<std::vector<deletable_handle_t>, QUEUES> _queue;

public:
    [[nodiscard]] uint32_t currentFrame() const { return _frame; }

    DeletionQueue(const DeletionQueue& other) = delete;
    DeletionQueue(DeletionQueue&& other) = default;
    DeletionQueue& operator=(const DeletionQueue& other) = delete;
    DeletionQueue& operator=(DeletionQueue&& other) = default;

#ifdef CONSTANT_DEBUG_MODE
    static void debug_check(const DeletionQueue* queue);
#define DEBUG_CHECK_DELETION_QUEUE(deletion_queue_ptr) DeletionQueue::debug_check(deletion_queue_ptr)
#else
#define DEBUG_CHECK_DELETION_QUEUE(deletion_queue_ptr) ((void)0)
#endif
};

}
