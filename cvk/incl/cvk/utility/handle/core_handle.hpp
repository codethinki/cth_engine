#pragma once
#include "destroy.hpp"
#include "unique_handle.hpp"

#include "cvk/base/allocator.hpp"
#include "cvk/base/core.hpp"
#include "cvk/base/device_table.hpp"
#include "cvk/utility/types.hpp"

#include <vk_mem_alloc.h>

#include <cth/ptr/ref_wrapper.hpp>


namespace cvk {
namespace dev {
    /**
     * `cvk::destroy()` with a `DeviceTable` and a `Handle` exists
     */
    template<class Handle>
    concept device_destroyable = requires(DeviceTable table, Handle handle) { cvk::destroy(table, handle); };

    /**
     * `cvk::destroy()` with a `VmaAllocator` and a `Handle` exists
     */
    template<class Handle>
    concept vma_destroyable = requires(VmaAllocator vmaAllocator, Handle handle) {
        cvk::destroy(vmaAllocator, handle);
    };
}

/**
 * deleter of the objects of a core
 * @details context: the core
 * @details calls `cvk::destroy()` with the device table or the allocator of the core, the one that fits the handle
 * @pre the core outlives the deleter
 */
class core_deleter {
public:
    constexpr explicit core_deleter(Core const& core) : _core{core} {}

    template<class Handle> requires dev::device_destroyable<Handle> || dev::vma_destroyable<Handle>
    constexpr void operator()(Handle handle) const {
        if constexpr(dev::device_destroyable<Handle>) cvk::destroy(_core->deviceTable(), handle);
        else cvk::destroy(_core->allocator().get(), handle);
    }

    [[nodiscard]] constexpr Core const& core() const { return *_core; }

private:
    cth::ref_wrapper<Core const> _core;
};

/**
 * `unique_handle` of an object of a core
 * @tparam Handle with an overload `cvk::destroy()` for a `DeviceTable` or a `VmaAllocator`
 */
template<class Handle>
using core_handle = unique_handle<Handle, core_deleter>;
}
