#pragma once
#include "queue.hpp"

#include <cth/coro/scheduler.hpp>

#include <cstddef>
#include <deque>
#include <vector>


namespace cvk {
class Core;
}

namespace jvk {
/**
 * serialized queues of a cvk core
 * @details run on an own scheduler
 * @details the indices of one vulkan queue share one @ref Queue
 * @details a @ref Queue keeps its address
 */
class Queues {
public:
    /**
     * constructs the queues of the core and starts their scheduler
     * @param workers threads of the scheduler, 0 -> one per vulkan queue
     * @pre @ref cvk::Core::created()
     */
    Queues(cvk::Core const&, size_t workers);

    /**
     * @param queueIdx index of the queue in @ref cvk::CoreConfig::queueProperties
     * @return serialized queue, the indices of one vulkan queue return the same object
     * @pre queueIdx < size of @ref cvk::Core::queueIndices()
     */
    [[nodiscard]] Queue const& queue(size_t queueIdx) const;

private:
    cth::co::scheduler _scheduler;
    // destroyed before the scheduler: their strands run on it
    std::deque<Queue> _queues{};

    /**
     * index in @ref _queues per index of @ref cvk::CoreConfig::queueProperties
     */
    std::vector<size_t> _queueIndices{};

public:
    Queues(Queues const& other) = delete;
    Queues(Queues&& other) = delete;
    Queues& operator=(Queues const& other) = delete;
    Queues& operator=(Queues&& other) = delete;
};
}
