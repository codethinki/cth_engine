#pragma once
#include "queue/queue_family.hpp"

namespace jvk {
struct DeviceConfig {
    /**
     * family indices of the queues to create
     * @see PhysicalDevice::Create
     */
    std::vector<queue_family_index_t> queueFamilyIndices;
};
}
