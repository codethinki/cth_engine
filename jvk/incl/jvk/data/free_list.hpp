#pragma once
#include <cstdint>
#include <optional>
#include <vector>


namespace jvk::dev {
/**
 * cells `[0, size)` of a heap area, hands out a free one
 * @details the last erased cell comes first, then the cells that were never in use
 * @attention not thread safe
 */
class free_list {
public:
    constexpr explicit free_list(uint32_t size) : _size{size} {}

    /**
     * @return a free cell, `std::nullopt` -> every cell is in use
     */
    [[nodiscard]] constexpr std::optional<uint32_t> insert() {
        if(!_erased.empty()) {
            auto const idx = _erased.back();
            _erased.pop_back();
            return idx;
        }
        if(_next == _size)
            return std::nullopt;
        return _next++;
    }

    /**
     * frees the cell
     * @pre `insert()` handed it out, no `erase()` since
     */
    constexpr void erase(uint32_t idx) { _erased.push_back(idx); }

    /**
     * @return the cells in use
     */
    [[nodiscard]] constexpr uint32_t used() const {
        return _next - static_cast<uint32_t>(_erased.size());
    }

    [[nodiscard]] constexpr uint32_t size() const { return _size; }

private:
    uint32_t _size;
    uint32_t _next = 0; // the cells from here on were never in use
    std::vector<uint32_t> _erased{};
};
}
