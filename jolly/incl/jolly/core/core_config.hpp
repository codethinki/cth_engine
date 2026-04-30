#pragma once

#include "jolly/render/submit/queue_properties.hpp"

#include <jvk/base/core_config.hpp>

namespace jly {
struct CoreConfig {
    std::string appName;
    std::string engineName;
    /**
     * Creates queues according to the properties. Queue[i] satisfies QueueProperties[i].
     * Tries to separate queue families if possible. Merges queues from the bottom up 
     *  by properties (1. equal 2. subset.eq, 3. combine all) if not enough distinct 
     *  queues are available. 
     */
    std::vector<QueueProperties> queues;

    size_t destructionQueueTickDelay;

    size_t schedulerThreads = 1;
    //TODO create a better system
    std::vector<std::string const> requiredExtensions = {};
};
}
