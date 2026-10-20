#pragma once
#include "cvk/utility/types.hpp"

#include <volk.h>

namespace cvk {
struct QueueState {
    /**
     * @brief function table of the device
     * @details stable when the device moves
     */
    not_null_ptr<VolkDeviceTable const> functions;
    vk_not_null<VkQueue> vkQueue;
    /**
     * @brief vulkan family index
     */
    uint32_t familyIndex;
    /**
     * @brief index in the family
     */
    uint32_t queueIndex;
};
}
