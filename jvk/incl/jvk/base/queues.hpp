#pragma once
#include "queue.hpp"

#include <cth/coro/scheduler.hpp>

#include <cstddef>
#include <deque>
#include <optional>
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
 * - their strands run on the workers of the core or on the driver threads, see `CoreConfig::driverThreads`
 * - the indices of one vulkan queue share one `Queue`
 * - a `Queue` keeps its address
 */
class Queues {
public:
    //TODO review the scheduler as a parameter like in TimelineReactor, no accessor of the workers on Core
    /**
     * constructs the queues of the core and the driver threads
     * @param workers run the strands for `driverThreads` `0`, the core passes its workers
     * @param driverThreads see `CoreConfig::driverThreads`
     * @pre the cvk core and the reactor of the core are created
     * @throws cvk::vk_result_exception result of `vkCreateSemaphore()` for the release counters
     */
    Queues(Core const&, cth::co::scheduler const& workers, size_t driverThreads);

    /**
     * @param queueIdx index of the queue in `cvk::CoreConfig::queueProperties`
     * @return serialized queue, the indices of one vulkan queue return the same object
     * @pre `queueIdx` < size of `cvk::Core::queueIndices()`
     */
    [[nodiscard]] Queue const& queue(size_t queueIdx) const;

private:
    // before the queues: destroyed after the strands that run on it
    std::optional<cth::co::scheduler> _driverSchedulerOpt{}; ///< the driver threads
    std::deque<Queue> _queues{};

    std::vector<size_t> _queueIndices{};
    ///< index in `_queues` per index of `cvk::CoreConfig::queueProperties`

public:
    Queues(Queues const& other) = delete;
    Queues(Queues&& other) = delete;
    Queues& operator=(Queues const& other) = delete;
    Queues& operator=(Queues&& other) = delete;
};
}
