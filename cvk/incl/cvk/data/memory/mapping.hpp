#pragma once
#include "cvk/utility/types.hpp"

#include <concepts>
#include <cstddef>
#include <span>


namespace cvk {
class Memory;

/**
 * @brief mapped range of a memory, the destructor unmaps
 * @tparam T element, std::byte for the bytes of a memory
 * @details move constructible, not copyable, not assignable, a moved from mapping does not unmap
 */
template<class T>
class Mapping {
public:
    /**
     * @brief takes over one map of the memory, @ref Memory::map() constructs it
     * @pre the range lies in the mapped memory
     */
    constexpr Mapping(Memory const& memory, std::span<T> range) : _memory{&memory}, _range{range} {}

    /**
     * @brief unmaps the memory once
     */
    ~Mapping();

    /**
     * @brief the range as elements of U, takes over the map
     * @details starts the lifetime of the elements, keeps their values
     * @pre the size is a multiple of sizeof(U), the data is aligned for U
     */
    template<class U> requires std::same_as<T, std::byte>
    [[nodiscard]] Mapping<U> as() &&;

    [[nodiscard]] constexpr std::span<T> span() const { return _range; }
    [[nodiscard]] constexpr T* data() const { return _range.data(); }
    [[nodiscard]] constexpr size_t size() const { return _range.size(); }
    [[nodiscard]] constexpr bool empty() const { return _range.empty(); }
    [[nodiscard]] constexpr auto begin() const { return _range.begin(); }
    [[nodiscard]] constexpr auto end() const { return _range.end(); }
    [[nodiscard]] constexpr T& operator[](size_t index) const { return _range[index]; }

private:
    move_ptr<Memory const> _memory;
    std::span<T> _range;

public:
    Mapping(Mapping const& other) = delete;
    Mapping(Mapping&& other) noexcept = default;
    Mapping& operator=(Mapping const& other) = delete;
    Mapping& operator=(Mapping&& other) = delete;
};
}
