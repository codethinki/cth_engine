#pragma once
#include "destroy.hpp"
#include "unique_handle.hpp"

#include "cvk/utility/types.hpp"

#include <volk.h>


namespace cvk {
/**
 * deleter of instance objects
 * @details context: the `VkInstance`, not the `Instance`
 */
class instance_deleter {
public:
    constexpr explicit instance_deleter(VkInstance vkInstance) : _vkInstance{vkInstance} {}

    template<class Handle>
    constexpr void operator()(Handle handle) const { cvk::destroy(_vkInstance, handle); }

    [[nodiscard]] constexpr VkInstance vkInstance() const { return _vkInstance.get(); }

private:
    vk_not_null<VkInstance> _vkInstance;
};

/**
 * `unique_handle` of an instance object
 * @tparam Handle with an overload `cvk::destroy(vk_not_null<VkInstance>, Handle)`
 */
template<class Handle>
using instance_handle = unique_handle<Handle, instance_deleter>;
}
