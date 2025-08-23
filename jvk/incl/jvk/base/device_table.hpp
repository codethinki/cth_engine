#pragma once
#include "jvk/utility/types.hpp"

#include <volk.h>

namespace jvk {
struct DeviceTable {
    jvk::vk_not_null<VkDevice> vkDevice;
    not_null<VolkDeviceTable const*> table;

    VolkDeviceTable const* operator ->() const { return table.get(); }
    [[nodiscard]] VkDevice device() const { return vkDevice.get(); }
};
}
