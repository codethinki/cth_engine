#pragma once
#include "vulkan/utility/cth_vk_types.hpp"

#include <volk.h>

namespace cth::vk {
struct DeviceTable {
    cth::vk::not_null<VkDevice> vkDevice;
    cth::not_null<VolkDeviceTable const*> table;

    VolkDeviceTable const* operator ->() const { return table.get(); }
    [[nodiscard]] VkDevice device() const { return vkDevice.get(); }
};
}
