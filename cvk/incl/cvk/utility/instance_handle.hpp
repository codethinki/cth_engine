#pragma once
#include "cvk/utility/types.hpp"
#include "cvk/utility/unique_handle.hpp"

#include <volk.h>


namespace cvk {
class Instance;
}

namespace cvk::dev {
[[nodiscard]] VkInstance vk_instance(Instance const&);
}

namespace cvk {
/**
 * @brief deleter of instance objects, context: the VkInstance
 * @details calls T::destroy(vkInstance, handle),
 *  stores the handle, not the Instance: the instance owns its messenger and can move
 */
template<class T>
class instance_deleter {
public:
    explicit instance_deleter(VkInstance vkInstance) : _vkInstance{vkInstance} {}
    explicit instance_deleter(Instance const& instance) : instance_deleter{dev::vk_instance(instance)} {}

    template<class Handle>
    void operator()(Handle handle) const { T::destroy(_vkInstance.get(), handle); }

    [[nodiscard]] VkInstance vkInstance() const { return _vkInstance.get(); }

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
