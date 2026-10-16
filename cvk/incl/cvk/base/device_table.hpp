#pragma once
#include "cvk/utility/types.hpp"

#include <volk.h>

namespace cvk {
struct DeviceTable {
    vk_not_null<VkDevice> vkDevice;
    not_null<VolkDeviceTable const*> table;

    VolkDeviceTable const* operator ->() const { return table.get(); }
    [[nodiscard]] VkDevice device() const { return vkDevice.get(); }
};
}
