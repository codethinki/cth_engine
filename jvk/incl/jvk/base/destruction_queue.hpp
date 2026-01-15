#pragma once
#include "destruction_queue_config.hpp"

#include <cth/io/log.hpp>

#include <functional>
#include <vector>


namespace jvk {
class DestructionQueue {
public:
    using function_t = std::function<void()>;
    using Config = DestructionQueueConfig;

    explicit DestructionQueue(Config);

    ~DestructionQueue();

    void push(function_t const& function);
    void push(std::span<function_t const> functions);

    void clear();
    void next();

private:
    void clearQueue();

    size_t _cycleSubIndex = 0;

    std::vector<std::vector<function_t>> _queue;

public:
    DestructionQueue(DestructionQueue const& other) = delete;
    DestructionQueue& operator=(DestructionQueue const& other) = delete;
    DestructionQueue(DestructionQueue&& other) noexcept = default;
    DestructionQueue& operator=(DestructionQueue&& other) noexcept = default;

    static void debug_check(DestructionQueue const& queue);
    static void debug_check_null_allowed(DestructionQueue const* queue);
};

}

//debug checks
namespace jvk {
inline void DestructionQueue::debug_check(DestructionQueue const& queue) {
}

inline void DestructionQueue::debug_check_null_allowed(DestructionQueue const* queue) {
    if(queue) debug_check(*queue);
}
}