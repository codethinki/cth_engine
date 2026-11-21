#pragma once
#include "destroy.hpp"
#include "unique_handle.hpp"

#include "cvk/utility/types.hpp"

#include <volk.h>

#include <cstddef>


namespace cvk {
/**
 * deleter of instance objects
 * @details
 * - context: the `VkInstance`, not the `Instance`
 * - the owner gives it with the handle, see `unique_handle::reset()`
 */
class instance_deleter {
public:
    // a nullptr constructor, no default constructor: the base constructor of the owner has no
    // created instance, the context comes with the handle; a reset() without context still does
    // not compile
    /**
     * without context, for an owner without a handle
     */
    constexpr explicit instance_deleter(std::nullptr_t) {}

    constexpr explicit instance_deleter(vk_not_null<VkInstance> vkInstance) : _vkInstance{
        vkInstance.get()
    } {}

    /**
     * @pre the deleter has a context
     */
    template<class Handle>
    constexpr void operator()(Handle handle) const { cvk::destroy(_vkInstance, handle); }

    /**
     * @return `VK_NULL_HANDLE` -> no context
     */
    [[nodiscard]] constexpr VkInstance vkInstance() const { return _vkInstance; }

private:
    VkInstance _vkInstance = VK_NULL_HANDLE;
};

/**
 * `unique_handle` of an instance object
 * @tparam Handle with an overload `cvk::destroy()` for a `vk_not_null<VkInstance>`
 */
template<class Handle>
using instance_handle = unique_handle<Handle, instance_deleter>;
}
