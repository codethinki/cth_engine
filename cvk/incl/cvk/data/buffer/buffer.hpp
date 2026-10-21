#pragma once
#include "base_buffer.hpp"

#include <volk.h>

#include <cth/io/log.hpp>
#include <cth/meta/concepts.hpp>

#include <algorithm>
#include <concepts>
#include <cstdint>
#include <memory>
#include <span>
#include <type_traits>


namespace cvk {
/**
 * @brief T is an index type of Vulkan 1.3 (VK_INDEX_TYPE_UINT8 needs an extension)
 */
template<class T>
concept index_element = std::same_as<T, uint16_t> || std::same_as<T, uint32_t>;

/**
 * @brief VkIndexType of T
 */
template<index_element T>
constexpr VkIndexType index_type = std::same_as<T, uint16_t> ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;

/**
 * @brief @ref BaseBuffer with an element type, sizes and offsets in elements
 * @tparam T element, the gpu copies it byte by byte
 * @details adds no members: a @ref BaseBuffer reference to it sees the same buffer in bytes
 */
template<cth::mta::trivially_copyable T>
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
     * @brief maps the range of the memory as elements
     * @param range in elements
     * @return the mapped elements, the destructor unmaps
     * @details calls @ref BaseBuffer::map()
     */
    [[nodiscard]] Mapping<T> map(Range range = {}) const { return BaseBuffer::map(bytes(range)).template as<T>(); }

    /**
     * @brief makes host writes visible to the device
     * @param range in elements
     * @details calls @ref BaseBuffer::flush()
     */
    void flush(Range range = {}) const { BaseBuffer::flush(bytes(range)); }

    /**
     * @brief makes device writes visible to the host
     * @param range in elements
     * @details calls @ref BaseBuffer::invalidate()
     */
    void invalidate(Range range = {}) const { BaseBuffer::invalidate(bytes(range)); }

    /**
     * @brief copies the data to the buffer
     * @param offset in elements
     * @details calls @ref BaseBuffer::write()
     */
    void write(std::span<T const> data, size_t offset = 0) const {
        BaseBuffer::write(std::as_bytes(data), offset * sizeof(T));
    }

    /**
     * @brief records a copy from the source buffer
     * @param region in elements
     * @details calls @ref BaseBuffer::copy()
     */
    void copy(BaseCmdBuffer const& cmdBuffer, Buffer const& src, BufferCopy region = {}) const {
        BaseBuffer::copy(
            cmdBuffer,
            src,
            {
                .srcOffset = region.srcOffset * sizeof(T),
                .dstOffset = region.dstOffset * sizeof(T),
                .size = bytes(region.size),
            }
        );
    }

    /**
     * @brief records a copy from the mip level of the image
     * @param dstOffset in elements
     * @details calls @ref BaseBuffer::copy(BaseCmdBuffer const&, Image const&, size_t, uint32_t, VkImageLayout) const
     */
    void copy(
        BaseCmdBuffer const& cmdBuffer,
        Image const& src,
        size_t dstOffset = 0,
        uint32_t mipLevel = 0,
        VkImageLayout layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
    ) const {
        BaseBuffer::copy(cmdBuffer, src, dstOffset * sizeof(T), mipLevel, layout);
    }

    [[nodiscard]] constexpr size_t elements() const { return size() / sizeof(T); }

private:
    /**
     * @param count (VK_WHOLE_SIZE => VK_WHOLE_SIZE)
     */
    [[nodiscard]] static constexpr size_t bytes(size_t count) {
        return count == VK_WHOLE_SIZE ? VK_WHOLE_SIZE : count * sizeof(T);
    }

    [[nodiscard]] static constexpr Range bytes(Range range) {
        return {.offset = range.offset * sizeof(T), .size = bytes(range.size)};
    }

public:
    Buffer(Buffer const& other) = delete;
    Buffer(Buffer&& other) noexcept = default;
    Buffer& operator=(Buffer const& other) = delete;
    Buffer& operator=(Buffer&& other) noexcept = default;
};
}
