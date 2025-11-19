#pragma once

#include <cth/data/union_find.hpp>

#include <span>
#include <string>
#include <string_view>


namespace jvk {
class Queue;
}

namespace jvk {
struct CoreConfig {
    using queue_set_t = cth::dt::union_find;

    std::string_view appName;
    std::string_view engineName;
    std::span<Queue> queues;
    std::span<std::string const> requiredExtensions; //TODO replace this with better extension handling

    /**
     * groups queues, every group gets its own unique vk_queue. 
     * @note empty -> default queue set, every queue is unique, equivalent to {0, ..., N - 1}
     * @details each root represents a group, requirements in a group are combined
     */
    std::span<queue_set_t const> queueSets{};

    /**
     * @brief if true, creates a DestructionQueue
     */
    bool destructionQueue = true;
};
}
