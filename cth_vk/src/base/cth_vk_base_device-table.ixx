module;
#include "lib/volk.hpp"

export module cth.vk.base.device_table;

import cth.vk.util.types;
import cth.ptr.not_null;


export namespace cth::vk {
struct DeviceTable {
    cth::vk::not_null<VkDevice> vkDevice;
    cth::not_null<VolkDeviceTable const*> table;

    VolkDeviceTable const* operator->() const { return table.get(); }
    [[nodiscard]] VkDevice device() const { return vkDevice.get(); }
};
}
