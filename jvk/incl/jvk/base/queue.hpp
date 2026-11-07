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
 * serialized vulkan queue
 * @details refers to the queue of the cvk core
 * @details runs its work in one strand
 */
class Queue {
public:
    /**
     * constructs the strand on the scheduler
     */
    Queue(cvk::Queue const&, cth::co::scheduler const&);

    /**
     * blocks until the queue finished its work
     * @attention deadlocks when called from a job of the strand
     * @throws cvk::vk_result_exception result of vkQueueWaitIdle()
     */
    void waitIdle() const;

    /**
     * @return executor of the strand
     */
    [[nodiscard]] constexpr cth::co::executor executor() const { return cth::co::executor{_strand}; }

    /**
     * @attention not serialized: use it inside the strand
     */
    [[nodiscard]] constexpr cvk::Queue const& vk() const { return *_queuePtr; }

    /**
     * @attention not serialized: use it inside the strand
     */
    [[nodiscard]] constexpr VkQueue get() const { return _queuePtr->get(); }

private:
    cth::not_null_ptr<cvk::Queue const> _queuePtr;
    cth::co::strand _strand;

public:
    Queue(Queue const& other) = delete;
    Queue(Queue&& other) = delete;
    Queue& operator=(Queue const& other) = delete;
    Queue& operator=(Queue&& other) = delete;
};
}
