#include "cvk/utility/core_handle.hpp"

#include "cvk/base/core.hpp"


namespace cvk::dev {
DeviceTable device_table(Core const& core) { return core.deviceTable(); }
}
