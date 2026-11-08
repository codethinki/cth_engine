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
class Core;
}

namespace jvk {
/**
 * serialized queues of a core
 * @details
 * - run on an own scheduler
 * - the indices of one vulkan queue share one `Queue`
 * - a `Queue` keeps its address
 */
class Queues {
public:
    /**
     * constructs the queues of the core and starts their scheduler
     * @param workers threads of the scheduler, `0` -> one per vulkan queue
     * @pre the cvk core and the reactor of the core are created
     */
    Queues(Core const&, size_t workers);

    //TEMP the old signature until queues.cpp implements the release, Core and the tests use it
    Queues(cvk::Core const&, size_t workers);

    /**
     * @param queueIdx index of the queue in `cvk::CoreConfig::queueProperties`
     * @return serialized queue, the indices of one vulkan queue return the same object
     * @pre `queueIdx` < size of `cvk::Core::queueIndices()`
     */
    [[nodiscard]] Queue const& queue(size_t queueIdx) const;

private:
    cth::co::scheduler _scheduler;
    // destroyed before the scheduler: their strands run on it
    std::deque<Queue> _queues{};

    std::vector<size_t> _queueIndices{}; ///< index in `_queues` per index of `cvk::CoreConfig::queueProperties`

public:
    Queues(Queues const& other) = delete;
    Queues(Queues&& other) = delete;
    Queues& operator=(Queues const& other) = delete;
    Queues& operator=(Queues&& other) = delete;
};
}
