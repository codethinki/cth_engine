#pragma once
#include "jvk/utility/constants.hpp"

#include <cth/io/log.hpp>

#include <array>
#include <functional>
#include <vector>


namespace jvk {
class DestructionQueue {
public:
    using function_t = std::function<void()>;

    explicit DestructionQueue() = default;
    ~DestructionQueue();

    void push(function_t const& function);
    void push(std::span<function_t const> functions);

    void clear();
    void next();

private:
    void clearQueue();

    static constexpr size_t QUEUES = constants::FRAMES_IN_FLIGHT * 2;

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

//debug checks
namespace jvk {
inline void DestructionQueue::debug_check(DestructionQueue const* queue) {
    CTH_ERR(queue == nullptr, "queue must not be nullptr")
    throw details->exception();
}

inline void DestructionQueue::debug_check_null_allowed(DestructionQueue const* queue) {
    if(queue) debug_check(queue);
}
}