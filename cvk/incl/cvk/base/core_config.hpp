#pragma once
#include "physical_device.hpp"
#include "queue/queue_family.hpp"

#include "cvk/utility/device/device_features.hpp"

#include <cth/data/union_find.hpp>
#include <cth/io/log.hpp>

#include <algorithm>
#include <optional>
#include <string>
#include <vector>


namespace cvk {
struct CoreConfig {
    using queue_set_t = cth::dt::union_find;

    /**
     * properties of each queue to create
     */
    std::vector<QueueFamilyProperties> queueProperties;

    /**
     * groups of queues, the first possible set is used
     * @details each root is a group, a group shares one vulkan queue
     * @details groups go to different queue families where possible
     * @details empty -> every queue is a group
     */
    std::vector<queue_set_t> queueSets{};

    /**
     * device features besides `constants::BASE_DEVICE_FEATURES`
     * @details a present queue adds `constants::PRESENT_DEVICE_FEATURES`
     */
    DeviceFeatures features{};

    /**
     * device extensions besides the extensions of the features
     * @details a present queue adds `constants::PRESENT_DEVICE_EXTENSIONS`
     */
    std::vector<std::string> extensions{};

    /**
     * `std::nullopt` -> the best suitable device
     * @details set -> exactly this device, see `PhysicalDevice::uuid()`
     */
    std::optional<device_uuid_t> device = std::nullopt;

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
