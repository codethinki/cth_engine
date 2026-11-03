#include "jvk/base/stasis.hpp"

#include "cvk/base/allocator.hpp"
#include "cvk/base/core.hpp"
#include "cvk/utility/handle/destroy.hpp"


namespace jvk {
Stasis::Stasis(cvk::Core const& core) : Stasis{core.deviceTable(), core.allocator().get()} {}

Stasis::Stasis(cvk::DeviceTable table, VmaAllocator vmaAllocator) : _table{table}, _allocator{vmaAllocator} {}

void Stasis::clear() {
    for(auto const& child : _poolChildren)
        std::visit(
            [this](auto const& grave) {
                std::apply([this](auto... handles) { cvk::destroy(_table, handles...); }, grave);
            },
            child
        );
    _poolChildren.clear();

    for(auto const& handle : _handles)
        std::visit(
            [this]<class Handle>(Handle grave) {
                if constexpr(requires { cvk::destroy(_table, grave); }) cvk::destroy(_table, grave);
                else cvk::destroy(_allocator, grave);
            },
            handle
        );
    _handles.clear();
}

Stasis& Stasis::operator=(Stasis&& other) noexcept {
    clear();

    _table = other._table;
    _allocator = other._allocator;
    _handles = std::move(other._handles);
    _poolChildren = std::move(other._poolChildren);
    return *this;
}
}
