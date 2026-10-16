#pragma once
#include "cvk/utility/types.hpp"
#include "cvk/utility/unique_handle.hpp"

#include <volk.h>


namespace cvk {
/**
 * @brief deleter of instance objects, context: the VkInstance
 * @details calls T::destroy(vkInstance, handle),
 *  stores the handle, not the Instance: the instance owns its messenger and can move
 */
template<class T>
class instance_deleter {
public:
    constexpr explicit instance_deleter(VkInstance vkInstance) : _vkInstance{vkInstance} {}

    template<class Handle>
    constexpr void operator()(Handle handle) const { T::destroy(_vkInstance.get(), handle); }

    [[nodiscard]] constexpr VkInstance vkInstance() const { return _vkInstance.get(); }

private:
    vk_not_null<VkInstance> _vkInstance;
};

/**
 * @brief @ref unique_handle of an instance object, the deleter stores the VkInstance
 * @tparam T owner class with a static destroy(VkInstance, Handle)
 */
template<class Handle, class T>
using instance_handle = unique_handle<Handle, instance_deleter<T>>;
}
