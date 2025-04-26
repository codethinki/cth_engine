export module cth.vk.res.destruction_queue;
import cth.vk.constants;


import std;

export namespace cth::vk {
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

    static void debug_check(DestructionQueue const& queue);
    static void debug_check_null_allowed(DestructionQueue const* queue);
};

}

//debug checks
export namespace cth::vk {
inline void DestructionQueue::debug_check(DestructionQueue const& queue) {}
inline void DestructionQueue::debug_check_null_allowed(DestructionQueue const* queue) { if(queue) debug_check(*queue); }
}
