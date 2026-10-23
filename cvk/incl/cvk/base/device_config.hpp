#pragma once
#include "queue/queue_family.hpp"

#include "cvk/utility/device/device_features.hpp"

#include <string>
#include <vector>

namespace cvk {
/**
 * @brief queues, features and extensions of a device
 * @details cvk needs @ref constants::BASE_DEVICE_FEATURES, @ref Core adds them
 */
struct DeviceConfig {
    /**
     * @brief family index of every queue to create
     */
    std::vector<queue_family_index_t> queueFamilyIndices;

    /**
     * @brief features to enable, a true bit of an extension struct enables its extension
     */
    DeviceFeatures features{};

    /**
     * @brief extensions to enable, besides the extensions of the features
     */
    std::vector<std::string> extensions{};
};
}
