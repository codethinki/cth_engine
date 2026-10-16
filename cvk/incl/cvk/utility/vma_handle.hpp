#pragma once
#include "cvk/utility/core_handle.hpp"
#include "cvk/utility/unique_handle.hpp"

#include <volk.h>
#include <vk_mem_alloc.h>


namespace cvk::dev {
[[nodiscard]] VmaAllocator vma_allocator(Core const&);
}

namespace cvk {
/**
 * @brief deleter of memory objects, context: the core
 * @details calls T::destroy(core.allocator().get(), handle)
 * @pre the core outlives the deleter
 */
template<class T>
class vma_deleter : public core_deleter<T> {
public:
    using core_deleter<T>::core_deleter;

    template<class Handle>
    void operator()(Handle handle) const { T::destroy(dev::vma_allocator(this->core()), handle); }
};

/**
 * @brief @ref unique_handle of a memory object, the deleter stores the core and frees through its allocator
 * @tparam T owner class with a static destroy(VmaAllocator, Handle)
 */
template<class Handle, class T>
using vma_handle = unique_handle<Handle, vma_deleter<T>>;
}
