#pragma once
#include "queue/queue_family.hpp"

#include "cvk/utility/device/device_features.hpp"

#include <cth/data/union_find.hpp>
#include <cth/io/log.hpp>

#include <algorithm>
#include <string>
#include <vector>


namespace cvk {
struct CoreConfig {
    using queue_set_t = cth::dt::union_find;

    /**
     * @brief properties of each queue to create
     */
    std::vector<QueueFamilyProperties> queueProperties;

    /**
     * @brief groups of queues, the first possible set is used, a group shares one vulkan queue
     * @details each root is a group, the properties of a group are combined,
     *  groups go to different queue families where possible
     * @note empty -> every queue is a group, same as {0, ..., N - 1}
     */
    std::vector<queue_set_t> queueSets{};

    /**
     * @brief device features besides @ref constants::BASE_DEVICE_FEATURES
     * @details a present queue adds @ref constants::PRESENT_DEVICE_FEATURES
     */
    DeviceFeatures features{};

    /**
     * @brief device extensions besides the extensions of the features
     * @details a present queue adds @ref constants::PRESENT_DEVICE_EXTENSIONS
     */
    std::vector<std::string> extensions{};

    static constexpr void debug_check(CoreConfig const& config);
};
}

namespace cvk {
constexpr void CoreConfig::debug_check(CoreConfig const& config) {
    CTH_CRITICAL(config.queueProperties.empty(), "at least one queue is required") {}
    CTH_CRITICAL(
        std::ranges::any_of(
            config.queueSets,
            [queues = config.queueProperties.size()](queue_set_t const& set) { return set.size() != queues; }
        ),
        "sets must span all queues"
    ) {}
}
}
