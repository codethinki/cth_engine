#pragma once
#include "unique_handle.hpp"

#include "cvk/base/core.hpp"
#include "cvk/utility/types.hpp"


namespace cvk {
/**
 * @brief deleter of device objects, context: the core
 * @details calls T::destroy(core.deviceTable(), handle)
 * @pre the core outlives the deleter
 */
template<vk_handle_owner T>
class core_deleter {
public:
    constexpr explicit core_deleter(Core const& core) : _core{&core} {}

    template<class Handle>
    constexpr void operator()(Handle handle) const { T::destroy(_core->deviceTable(), handle); }

    [[nodiscard]] constexpr Core const& core() const { return *_core; }

private:
    not_null_ptr<Core const> _core;
};

/**
 * @brief @ref unique_handle of a device object, the deleter stores the core
 * @tparam T owner class with vk_handle_t and a static destroy(DeviceTable, vk_handle_t)
 */
template<vk_handle_owner T>
using core_handle = unique_handle<typename T::vk_handle_t, core_deleter<T>>;
}
