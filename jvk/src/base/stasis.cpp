#include "jvk/base/stasis.hpp"

#include "cvk/base/allocator.hpp"
#include "cvk/base/core.hpp"
#include "cvk/utility/handle/destroy.hpp"

#include <cth/io/log.hpp>

#include <cstddef>
#include <ranges>
#include <tuple>
#include <utility>
#include <variant>


namespace jvk {
namespace {
    [[nodiscard]] constexpr size_t count_before(size_t position, size_t cleared, size_t size) {
        CTH_CRITICAL(
            position > cleared + size,
            "the position must come from end(), got {} > {}",
            position,
            cleared + size
        ) {}
        return position > cleared ? position - cleared : 0;
    }
}

Stasis::Stasis(cvk::Core const& core) : Stasis{core.deviceTable(), core.allocator().get()} {}

Stasis::Stasis(cvk::DeviceTable table, VmaAllocator vmaAllocator) : _table{table},
    _vmaAllocator{vmaAllocator} {}

void Stasis::clear(pos const& position) {
    auto const pooled = count_before(
        position.pooledHandles,
        _clearedPooledHandles,
        _pooledHandles.size()
    );
    auto const handles = count_before(position.handles, _clearedHandles, _handles.size());

    for(auto const& pooledHandle : _pooledHandles | std::views::take(pooled))
        std::visit(
            [this](auto const& grave) {
                std::apply(
                    [this](auto... vkHandles) { cvk::destroy(_table, vkHandles...); },
                    grave
                );
            },
            pooledHandle
        );
    _pooledHandles.erase(
        _pooledHandles.begin(),
        _pooledHandles.begin() + static_cast<ptrdiff_t>(pooled)
    );
    _clearedPooledHandles += pooled;

    for(auto const& handle : _handles | std::views::take(handles))
        std::visit(
            [this]<class Handle>(Handle grave) {
                if constexpr(requires { grave.erase(); })
                    grave.erase();
                else if constexpr(requires { cvk::destroy(_table, grave); })
                    cvk::destroy(_table, grave);
                else
                    cvk::destroy(_vmaAllocator, grave);
            },
            handle
        );
    _handles.erase(_handles.begin(), _handles.begin() + static_cast<ptrdiff_t>(handles));
    _clearedHandles += handles;
}

Stasis& Stasis::operator=(Stasis&& other) noexcept {
    clear();

    _table = other._table;
    _vmaAllocator = other._vmaAllocator;
    _handles = std::move(other._handles);
    _pooledHandles = std::move(other._pooledHandles);
    _clearedHandles = other._clearedHandles;
    _clearedPooledHandles = other._clearedPooledHandles;
    return *this;
}
}
