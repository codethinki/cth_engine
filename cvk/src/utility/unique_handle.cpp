#include "cvk/utility/unique_handle.hpp"

#include "cvk/base/core.hpp"
#include "cvk/base/instance.hpp"


namespace cvk::dev {
DeviceTable device_table(Core const& core) { return core.deviceTable(); }

VkInstance vk_instance(Instance const& instance) { return instance.get(); }
}
