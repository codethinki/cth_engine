#pragma once
#include "cvk/utility/types.hpp"

#include <volk.h>

#include <cth/ptr/ref_wrapper.hpp>

namespace cvk {
struct QueueState {
    /**
     * function table of the device
     * @details stable when the device moves
     */
    cth::ref_wrapper<VolkDeviceTable const> functions;
    vk_not_null<VkQueue> vkQueue;
    uint32_t familyIndex; ///< vulkan family index
    uint32_t queueIndex; ///< index in the family
};
}
