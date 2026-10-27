#pragma once
#include "unique_handle.hpp"

#include "cvk/base/device_table.hpp"
#include "cvk/utility/types.hpp"

#include <volk.h>
#include <vk_mem_alloc.h>

#include <concepts>
#include <tuple>
#include <utility>


namespace cvk {
/**
 * call of T::destroy() with its context and the handle, filled in advance
 * @details aggregate, only the call destroys: the destructor does nothing
 * @tparam T owner class with vk_handle_t and a static destroy(Context..., vk_handle_t)
 * @tparam Context what T::destroy() needs besides the handle, may be empty
 */
template<vk_handle_owner T, class... Context>
struct basic_deletable {
    [[no_unique_address]] std::tuple<Context...> context;
    vk_not_null<typename T::vk_handle_t> handle;

    /**
     * destroys the handle
     * @attention call it once, a second call destroys the handle again
     */
    void operator()() && {
        std::apply([this](Context... args) { T::destroy(args..., handle.get()); }, std::move(context));
    }
};

/**
 * deletable of a device object, context: the device table
 * @details calls T::destroy(table, handle), 3 pointers
 */
template<vk_handle_owner T>
struct device_deletable : basic_deletable<T, DeviceTable> {
    [[nodiscard]] constexpr DeviceTable table() const { return std::get<0>(this->context); }
};

/**
 * deletable of a memory object, context: the allocator
 * @details calls T::destroy(vmaAllocator, handle)
 */
template<vk_handle_owner T>
struct vma_deletable : basic_deletable<T, VmaAllocator> {
    [[nodiscard]] constexpr VmaAllocator allocator() const { return std::get<0>(this->context); }
};

/**
 * deletable of an instance object, context: the VkInstance
 * @details calls T::destroy(vkInstance, handle)
 */
template<vk_handle_owner T>
struct instance_deletable : basic_deletable<T, vk_not_null<VkInstance>> {
    [[nodiscard]] constexpr VkInstance vkInstance() const { return std::get<0>(this->context).get(); }
};

/**
 * what T::State::collapse() turns the state into
 */
template<class T>
using deletable_t = decltype(std::declval<typename T::State>().collapse());

/**
 * the call of the deletable of a T destroys what the T owned
 */
template<class T>
concept destroyable = std::invocable<deletable_t<T>>;
}
