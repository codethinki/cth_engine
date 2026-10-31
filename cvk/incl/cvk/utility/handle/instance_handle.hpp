#pragma once
#include "destroy.hpp"
#include "unique_handle.hpp"

#include "cvk/utility/types.hpp"

#include <volk.h>


namespace cvk {
/**
 * deleter of instance objects, context: the VkInstance
 * @details calls @ref cvk::destroy(vkInstance, handle),
 *  stores the handle, not the Instance: the instance owns its messenger and can move
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
 * @ref unique_handle of an instance object, the deleter stores the VkInstance
 * @tparam Handle with an overload @ref cvk::destroy(vk_not_null<VkInstance>, Handle)
 */
template<class Handle>
using instance_handle = unique_handle<Handle, instance_deleter>;
}
