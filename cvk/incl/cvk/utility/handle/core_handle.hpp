#pragma once
#include "destroy.hpp"
#include "unique_handle.hpp"

#include "cvk/base/allocator.hpp"
#include "cvk/base/core.hpp"
#include "cvk/base/device_table.hpp"
#include "cvk/utility/types.hpp"

#include <vk_mem_alloc.h>


namespace cvk {
namespace dev {
    /**
     * @ref cvk::destroy(DeviceTable, Handle) exists
     */
    template<class Handle>
    concept device_destroyable = requires(DeviceTable table, Handle handle) { cvk::destroy(table, handle); };

    /**
     * @ref cvk::destroy(VmaAllocator, Handle) exists
     */
    template<class Handle>
    concept vma_destroyable = requires(VmaAllocator vmaAllocator, Handle handle) {
        cvk::destroy(vmaAllocator, handle);
    };
}

/**
 * deleter of the objects of a core, context: the core
 * @details calls @ref cvk::destroy() with the context of the core that fits the handle:
 *  core.deviceTable() or core.allocator().get()
 * @pre the core outlives the deleter
 */
class core_deleter {
public:
    constexpr explicit core_deleter(Core const& core) : _core{&core} {}

    template<class Handle> requires dev::device_destroyable<Handle> || dev::vma_destroyable<Handle>
    constexpr void operator()(Handle handle) const {
        if constexpr(dev::device_destroyable<Handle>) cvk::destroy(_core->deviceTable(), handle);
        else cvk::destroy(_core->allocator().get(), handle);
    }

    [[nodiscard]] constexpr Core const& core() const { return *_core; }

private:
    not_null_ptr<Core const> _core;
};

/**
 * @ref unique_handle of an object of a core, the deleter stores the core
 * @tparam Handle with an overload @ref cvk::destroy(DeviceTable, Handle) or @ref cvk::destroy(VmaAllocator, Handle)
 */
template<class Handle>
using core_handle = unique_handle<Handle, core_deleter>;
}
