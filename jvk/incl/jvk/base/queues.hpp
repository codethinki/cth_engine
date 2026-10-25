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
 * serialized queues of a cvk core on an own scheduler, neither copyable nor movable
 * @details one @ref Queue per vulkan queue of the roles, roles with the same vulkan queue share one @ref Queue,
 *  a @ref Queue keeps its address
 */
class Queues {
public:
    /**
     * starts the scheduler, constructs one @ref Queue per vulkan queue of the roles
     * @param workers threads of the scheduler, 0 -> one per vulkan queue
     * @pre @ref cvk::Core::created()
     */
    Queues(cvk::Core const&, size_t workers);

    /**
     * @param role index in @ref cvk::CoreConfig::queueProperties
     * @return serialized queue of the role, roles with the same vulkan queue return the same object
     * @pre role < size of @ref cvk::Core::queueIndices()
     */
    [[nodiscard]] Queue const& queue(size_t role) const;

private:
    cth::co::scheduler _scheduler;
    // destroyed before the scheduler: their strands run on it
    std::deque<Queue> _queues{};

    /**
     * index in @ref _queues per role
     */
    std::vector<size_t> _queueIndices{};

public:
    Queues(Queues const& other) = delete;
    Queues(Queues&& other) = delete;
    Queues& operator=(Queues const& other) = delete;
    Queues& operator=(Queues&& other) = delete;
};
}
