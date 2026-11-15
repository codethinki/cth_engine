#pragma once
#include <volk.h>

#include <compare>
#include <cstddef>
#include <type_traits>


namespace cvk {
/**
 * typed address of gpu memory
 * @details
 * - the arithmetic steps in elements of `T`, like a pointer
 * - `0` -> null, the default
 * - no dereference and no `operator[]`: the host cannot read the element
 * - 8 bytes and trivially copyable: a member of push data, matches a GLSL `buffer_reference` member
 * @tparam T element at the address
 */
template<class T>
class device_ptr {
public:
    constexpr device_ptr() = default;
    constexpr explicit device_ptr(VkDeviceAddress address) : _address{address} {}

    [[nodiscard]] constexpr VkDeviceAddress get() const { return _address; }

    constexpr device_ptr& operator+=(std::ptrdiff_t count) {
        _address += static_cast<VkDeviceAddress>(count) * sizeof(T);
        return *this;
    }
    constexpr device_ptr& operator-=(std::ptrdiff_t count) {
        _address -= static_cast<VkDeviceAddress>(count) * sizeof(T);
        return *this;
    }

    [[nodiscard]] friend constexpr device_ptr operator+(device_ptr ptr, std::ptrdiff_t count) { return ptr += count; }
    [[nodiscard]] friend constexpr device_ptr operator+(std::ptrdiff_t count, device_ptr ptr) { return ptr += count; }
    [[nodiscard]] friend constexpr device_ptr operator-(device_ptr ptr, std::ptrdiff_t count) { return ptr -= count; }

    /**
     * @return distance in elements
     */
    [[nodiscard]] friend constexpr std::ptrdiff_t operator-(device_ptr a, device_ptr b) {
        return static_cast<std::ptrdiff_t>(a._address - b._address) / static_cast<std::ptrdiff_t>(sizeof(T));
    }

    [[nodiscard]] constexpr bool operator==(device_ptr const& other) const = default;
    [[nodiscard]] constexpr auto operator<=>(device_ptr const& other) const = default;

private:
    VkDeviceAddress _address = 0;
};

static_assert(sizeof(device_ptr<std::byte>) == 8, "a device_ptr must be 8 bytes: a GLSL buffer_reference member");
static_assert(std::is_trivially_copyable_v<device_ptr<std::byte>>, "a device_ptr must fit into push data");
}
