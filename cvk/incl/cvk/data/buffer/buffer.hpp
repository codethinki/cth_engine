#pragma once
#include "base_buffer.hpp"

#include <volk.h>

#include <cth/io/log.hpp>

#include <algorithm>
#include <memory>
#include <span>
#include <type_traits>


namespace cvk {
/**
 * @brief @ref BaseBuffer with an element type, sizes and offsets in elements
 * @tparam T element, the gpu copies it byte by byte
 * @details adds no members: a @ref BaseBuffer reference to it sees the same buffer in bytes
 */
template<class T> requires std::is_trivially_copyable_v<T>
class Buffer : public BaseBuffer {
public:
    /**
     * @brief base constructor
     * @pre @ref Core::created()
     */
    Buffer(Core const& core, Config const& config) : BaseBuffer{core, config} {}

    /**
     * @brief constructs and wraps
     * @details calls:
     * - @ref Buffer(Core const&, Config const&)
     * - @ref wrap()
     */
    Buffer(Core const& core, Config const& config, State const& state) : Buffer{core, config} { wrap(state); }

    /**
     * @brief constructs and creates
     * @details calls:
     * - @ref Buffer(Core const&, Config const&)
     * - @ref create()
     */
    Buffer(Core const& core, Config const& config, size_t count) : Buffer{core, config} { create(count); }

    /**
     * @brief wraps the state
     * @pre @ref State::size is a multiple of sizeof(T)
     * @details calls @ref BaseBuffer::wrap()
     */
    void wrap(State const& state) {
        CTH_CRITICAL(state.size % sizeof(T) != 0, "size must be a multiple of the element size") {}
        BaseBuffer::wrap(state);
    }

    /**
     * @brief creates the buffer and its memory
     * @param count of elements
     * @details calls @ref BaseBuffer::create()
     */
    void create(size_t count) { BaseBuffer::create(count * sizeof(T)); }

    /**
     * @brief maps the memory as elements
     * @param count (VK_WHOLE_SIZE => to the end)
     * @param offset in elements
     * @return the mapped elements
     * @pre offset + count <= @ref elements()
     * @note each call needs one @ref unmap()
     * @details calls @ref Memory::map()
     */
    [[nodiscard]] std::span<T> map(size_t count = VK_WHOLE_SIZE, size_t offset = 0) const {
        CTH_CRITICAL(offset > elements(), "offset must lie in the buffer") {}
        size_t const mapCount = count == VK_WHOLE_SIZE ? elements() - offset : count;
        CTH_CRITICAL(mapCount > elements() - offset, "range must lie in the buffer") {}

        return as_elements(memory().map(mapCount * sizeof(T), offset * sizeof(T)));
    }

    /**
     * @brief unmaps the memory once
     * @details calls @ref Memory::unmap()
     */
    void unmap() const { memory().unmap(); }

    /**
     * @brief copies the data to the buffer
     * @param offset in elements
     * @pre offset + data.size() <= @ref elements()
     * @details calls @ref Memory::write()
     */
    void write(std::span<T const> data, size_t offset = 0) const {
        CTH_CRITICAL(offset > elements() || data.size() > elements() - offset, "range must lie in the buffer") {}

        memory().write(std::span{reinterpret_cast<char const*>(data.data()), data.size_bytes()}, offset * sizeof(T));
    }

    /**
     * @brief makes host writes visible to the device
     * @param count (VK_WHOLE_SIZE => to the end)
     * @param offset in elements
     * @details calls @ref Memory::flush()
     */
    void flush(size_t count = VK_WHOLE_SIZE, size_t offset = 0) const {
        memory().flush(bytes(count), offset * sizeof(T));
    }

    /**
     * @brief makes device writes visible to the host
     * @param count (VK_WHOLE_SIZE => to the end)
     * @param offset in elements
     * @details calls @ref Memory::invalidate()
     */
    void invalidate(size_t count = VK_WHOLE_SIZE, size_t offset = 0) const {
        memory().invalidate(bytes(count), offset * sizeof(T));
    }

    /**
     * @param count (VK_WHOLE_SIZE => to the end)
     * @param offset in elements
     * @pre offset + count <= @ref elements()
     * @details calls @ref BaseBuffer::descriptorInfo()
     */
    [[nodiscard]] constexpr VkDescriptorBufferInfo descriptorInfo(
        size_t count = VK_WHOLE_SIZE,
        size_t offset = 0
    ) const {
        return BaseBuffer::descriptorInfo(bytes(count), offset * sizeof(T));
    }

    /**
     * @brief copies the data to the mapped elements
     * @pre data.size() <= mapped.size()
     */
    static constexpr void write(std::span<T const> data, std::span<T> mapped) {
        CTH_CRITICAL(data.size() > mapped.size(), "data must fit into the mapped range") {}
        std::ranges::copy(data, mapped.begin());
    }

    [[nodiscard]] constexpr size_t elements() const { return size() / sizeof(T); }

private:
    /**
     * @param count (VK_WHOLE_SIZE => VK_WHOLE_SIZE)
     */
    [[nodiscard]] static constexpr size_t bytes(size_t count) {
        return count == VK_WHOLE_SIZE ? VK_WHOLE_SIZE : count * sizeof(T);
    }

    /**
     * @brief starts the lifetime of the elements in the mapped bytes, keeps their values
     */
    [[nodiscard]] static std::span<T> as_elements(std::span<char> mapped) {
        size_t const count = mapped.size() / sizeof(T);
#ifdef __cpp_lib_start_lifetime_as
        return {std::start_lifetime_as_array<T>(mapped.data(), count), count};
#else
        //TODO remove when the MSVC STL has start_lifetime_as on clang (microsoft/STL GH-6169)
        return {reinterpret_cast<T*>(mapped.data()), count};
#endif
    }

public:
    Buffer(Buffer const& other) = delete;
    Buffer(Buffer&& other) noexcept = default;
    Buffer& operator=(Buffer const& other) = delete;
    Buffer& operator=(Buffer&& other) noexcept = default;
};
}
