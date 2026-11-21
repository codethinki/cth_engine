#pragma once
#include <cstdint>


namespace jvk {
/**
 * index of a handle in its heap, for the push data
 * @details the index of the `jvk_*` arrays of `jvk/heaps.glsl`, see `cvk::device_ptr` for buffers
 * @tparam Handle type of the handle, keeps the indices of different types apart
 */
template<class Handle>
class heap_idx {
public:
    constexpr heap_idx() = default;

    constexpr explicit heap_idx(uint32_t idx) : _idx{idx} {}

    [[nodiscard]] constexpr uint32_t get() const { return _idx; }

    [[nodiscard]] constexpr bool operator==(heap_idx const&) const = default;

private:
    uint32_t _idx = 0;
};
}
