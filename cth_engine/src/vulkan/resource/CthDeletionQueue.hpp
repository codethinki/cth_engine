#pragma once
#include "vulkan/utility/cth_constants.hpp"

#include <cth/cth_type_trait.hpp>
#include <vulkan/vulkan.h>

#include <array>
#include <variant>
#include <vector>


namespace cth::vk {
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


    void clear(size_t frame_index);
    void next(size_t const next_frame) { _frameIndex = next_frame; }

private:
    using handle_t = std::variant<
        deletable_handle_t,
        dependent_handle_t
    >;
    struct deletable {
        handle_t handle;
        deletable_handle_t dependency;
    };

    static constexpr size_t QUEUES = constants::FRAMES_IN_FLIGHT;

    size_t _frameIndex = 0;
    BasicCore* _core; //temp make this const_ptr

    std::array<std::vector<deletable>, QUEUES> _queue;

public:
    [[nodiscard]] uint32_t currentFrame() const { return _frameIndex; }

    DeletionQueue(DeletionQueue const& other) = delete;
    DeletionQueue(DeletionQueue&& other) = default;
    DeletionQueue& operator=(DeletionQueue const& other) = delete;
    DeletionQueue& operator=(DeletionQueue&& other) = default;

#ifdef CONSTANT_DEBUG_MODE
    static void debug_check(DeletionQueue const* queue);
    static void debug_check_null_allowed(DeletionQueue const* queue);
#define DEBUG_CHECK_DELETION_QUEUE(deletion_queue_ptr) DeletionQueue::debug_check(deletion_queue_ptr)
#define DEBUG_CHECK_DELETION_QUEUE_NULL_ALLOWED(deletion_queue_ptr) 
#else
#define DEBUG_CHECK_DELETION_QUEUE(deletion_queue_ptr) ((void)0)
#define DEBUG_CHECK_DELETION_QUEUE_NULL_ALLOWED(deletion_queue_ptr) ((void)0) 
#endif
};

}
