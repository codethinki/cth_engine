#pragma once
#include "cvk/base/device_table.hpp"
#include "cvk/utility/types.hpp"
#include "cvk/utility/unique_handle.hpp"


namespace cvk {
class Core;
}

namespace cvk::dev {
[[nodiscard]] DeviceTable device_table(Core const&);
}

namespace cvk {
/**
 * @brief deleter of device objects, context: the core
 * @details calls T::destroy(core.deviceTable(), handle)
 * @pre the core outlives the deleter
 */
template<class T>
class core_deleter {
public:
    explicit core_deleter(Core const& core) : _core{&core} {}

    template<class Handle>
    void operator()(Handle handle) const { T::destroy(dev::device_table(*_core), handle); }

    [[nodiscard]] Core const& core() const { return *_core; }

private:
    not_null<Core const*> _core;
};

/**
 * @brief @ref unique_handle of a device object, the deleter stores the core
 * @tparam T owner class with a static destroy(DeviceTable, Handle)
 */
template<class Handle, class T>
using core_handle = unique_handle<Handle, core_deleter<T>>;
}
