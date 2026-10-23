#pragma once

#include "destruction_queue_config.hpp"

#include "queue/queue_family.hpp"

#include <cth/data/union_find.hpp>

#include <string>


namespace jvk {
class Queue;
}

namespace jvk {
struct CoreConfig {
    using queue_set_t = cth::dt::union_find;

    std::string appName;
    std::string engineName;
    std::vector<std::string> requiredInstanceExtensions; //TODO replace this with better extension handling


    /**
     * creates a destruction queue if not @ref std::nullopt 
     */
    std::optional<DestructionQueueConfig> destructionQueueConfig;

    /**
     * defines the properties for the created queues
     * 
     */
    std::vector<QueueFamilyProperties> queueProperties;

    /**
     * Groups queues, first possible queue set is accepted. Groups share the same vk_queue. 
     * @note empty -> default queue set, every queue is unique, equivalent to {0, ..., N - 1}
     * @details each root represents a group, requirements in a group are combined.
     *  Groups are separated onto different queue families as much as possible.
     */
    std::vector<queue_set_t> queueSets{};

    static constexpr void debug_check(CoreConfig const&);
};
constexpr void CoreConfig::debug_check(CoreConfig const& c) {
    auto const& sets = c.queueSets;

    CTH_CRITICAL(
        std::ranges::any_of(sets, [queues = c.queueProperties.size()](auto& set){ return set.size() != queues; }),
        "sets must span all queues"
    ){}
}
}
