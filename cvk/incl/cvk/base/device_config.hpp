#pragma once
#include "queue/queue_family.hpp"

#include "cvk/utility/device/device_features.hpp"

#include <string>
#include <vector>

namespace cvk {
/**
 * config of a `Device`
 * @details `Core` adds `constants::BASE_DEVICE_FEATURES`
 */
struct DeviceConfig {
    std::vector<queue_family_index_t> queueFamilyIndices; ///< family index of every queue to create

    DeviceFeatures features{}; ///< features to enable, a `true` bit of an extension struct enables its extension

    std::vector<std::string> extensions{}; ///< extensions to enable, besides the extensions of the features
};
}
