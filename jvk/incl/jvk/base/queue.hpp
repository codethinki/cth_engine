#pragma once
#include "cvk/base/queue/queue.hpp"

#include <volk.h>

#include <cth/coro/executor.hpp>
#include <cth/coro/strand.hpp>
#include <cth/ptr/not_null.hpp>


namespace cth::co {
class scheduler;
}

namespace jvk {
/**
 * serialized vulkan queue: its queue work runs in one strand, neither copyable nor movable
 * @details refers to the queue of the cvk core,
 *  Vulkan requires one thread at a time per vulkan queue,
 *  @ref Queues constructs the queues
 */
class Queue {
public:
    using vk_handle_t = VkQueue;

    /**
     * constructs the strand on the scheduler
     */
    Queue(cvk::Queue const&, cth::co::scheduler const&);

    /**
     * blocks until the queue finished its work
     * @details runs vkQueueWaitIdle() inside the strand
     * @attention deadlocks when called from a job of the strand
     * @throws cvk::vk_result_exception result of vkQueueWaitIdle()
     */
    void waitIdle() const;

    /**
     * @return executor of the strand, co_await executor().schedule() continues inside the strand
     */
    [[nodiscard]] constexpr cth::co::executor executor() const { return cth::co::executor{_strand}; }

    /**
     * @attention not serialized: use it inside the strand
     */
    [[nodiscard]] constexpr cvk::Queue const& vk() const { return *_queue; }

    /**
     * @attention not serialized: use it inside the strand
     */
    [[nodiscard]] constexpr vk_handle_t get() const { return _queue->get(); }

private:
    cth::not_null_ptr<cvk::Queue const> _queue;
    cth::co::strand _strand;

public:
    Queue(Queue const& other) = delete;
    Queue(Queue&& other) = delete;
    Queue& operator=(Queue const& other) = delete;
    Queue& operator=(Queue&& other) = delete;
};
}
