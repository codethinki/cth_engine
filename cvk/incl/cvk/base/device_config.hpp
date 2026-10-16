#pragma once
#include "queue/queue_family.hpp"

#include <vector>

namespace cvk {
struct DeviceConfig {
    /**
     * @brief family index of every queue to create
     * @details see @ref PhysicalDevice::Create()
     */
    std::vector<queue_family_index_t> queueFamilyIndices;
};
}
