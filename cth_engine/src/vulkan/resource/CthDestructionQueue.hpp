#pragma once
#include "vulkan/utility/cth_constants.hpp"

#include <array>
#include <vector>



namespace cth::vk {
class DestructionQueue {
public:
    using function_t = std::function<void()>;

    explicit DestructionQueue() = default;
    ~DestructionQueue();

    void push(function_t const& function);
    void push(std::span<function_t const> functions);

    void clear();
    void clear(size_t cycle_sub_index);

private:
    static constexpr size_t QUEUES = constants::FRAMES_IN_FLIGHT;

    size_t _cycleSubIndex = 0;

    std::array<std::vector<function_t>, QUEUES> _queue;

public:
    DestructionQueue(DestructionQueue const& other) = delete;
    DestructionQueue& operator=(DestructionQueue const& other) = delete;
    DestructionQueue(DestructionQueue&& other) noexcept = default;
    DestructionQueue& operator=(DestructionQueue&& other) noexcept = default;

#ifdef CONSTANT_DEBUG_MODE
    static void debug_check(DestructionQueue const* queue);
    static void debug_check_null_allowed(DestructionQueue const* queue);
#define DEBUG_CHECK_DESTRUCTION_QUEUE(destruction_queue_ptr) DestructionQueue::debug_check(destruction_queue_ptr)
#define DEBUG_CHECK_DESTRUCTION_QUEUE_NULL_ALLOWED(destruction_queue_ptr) 
#else
#define DEBUG_CHECK_DESTRUCTION_QUEUE(destruction_queue_ptr) ((void)0)
#define DEBUG_CHECK_DESTRUCTION_QUEUE_NULL_ALLOWED(destruction_queue_ptr) ((void)0) 
#endif
};

}
