#pragma once
#include "cvk/utility/types.hpp"

#include <volk.h>

#include <cth/ptr/ref_wrapper.hpp>

namespace cvk {
struct DeviceTable {
    vk_not_null<VkDevice> vkDevice;
    cth::ref_wrapper<VolkDeviceTable const> table;

    VolkDeviceTable const* operator ->() const { return &table.get(); }
    [[nodiscard]] constexpr VkDevice device() const { return vkDevice.get(); }
};
}
