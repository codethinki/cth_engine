#pragma once
#include "core_handle.hpp"
#include "unique_handle.hpp"

#include "cvk/base/allocator.hpp"
#include "cvk/base/core.hpp"

#include <volk.h>
#include <vk_mem_alloc.h>


namespace cvk {
/**
 * @brief deleter of memory objects, context: the core
 * @details calls T::destroy(core.allocator().get(), handle)
 * @pre the core outlives the deleter
 */
template<vk_handle_owner T>
class vma_deleter : public core_deleter<T> {
public:
    using core_deleter<T>::core_deleter;

    template<class Handle>
    constexpr void operator()(Handle handle) const { T::destroy(this->core().allocator().get(), handle); }
};

/**
 * @brief @ref unique_handle of a memory object, the deleter stores the core and frees through its allocator
 * @tparam T owner class with vk_handle_t and a static destroy(VmaAllocator, vk_handle_t)
 */
template<vk_handle_owner T>
using vma_handle = unique_handle<typename T::vk_handle_t, vma_deleter<T>>;
}
