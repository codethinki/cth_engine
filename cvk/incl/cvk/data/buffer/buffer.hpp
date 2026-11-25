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
 * `T` is an index type of Vulkan 1.3
 */
template<class T>
concept index_element = std::same_as<T, uint16_t> || std::same_as<T, uint32_t>;

/**
 * `VkIndexType` of `T`
 */
template<index_element T>
constexpr VkIndexType index_type = std::same_as<T, uint16_t>
    ? VK_INDEX_TYPE_UINT16
    : VK_INDEX_TYPE_UINT32;

/**
 * typed buffer
 * @details sizes and offsets in elements
 * @details a `BaseBuffer` reference sees the same buffer in bytes
 * @tparam T element
 */
template<cth::mta::trivially_copyable T>
class Buffer : public BaseBuffer {
public:
    /**
     * base constructor
     */
    Buffer(Core const& core, Config const& config) : BaseBuffer{core, config} {}

    /**
     * constructs and wraps
     * @details calls @ref Buffer(Core const&, Config const&), `wrap()`
     */
    Buffer(Core const& core, Config const& config, State const& state) : Buffer{core, config} {
        wrap(state);
    }

    /**
     * constructs and creates
     * @details calls @ref Buffer(Core const&, Config const&), `create()`
     */
    Buffer(Core const& core, Config const& config, size_t count) : Buffer{core, config} {
        create(count);
    }

    /**
     * wraps the state
     * @pre `BaseBuffer::State::size` is a multiple of `sizeof(T)`
     */
    void wrap(State const& state) {
        CTH_CRITICAL(state.size % sizeof(T) != 0, "size must be a multiple of the element size") {}
        BaseBuffer::wrap(state);
    }

    /**
     * creates the buffer and its memory
     * @param count of elements
     */
    void create(size_t count) { BaseBuffer::create(count * sizeof(T)); }

    /**
     * maps the range of the memory as elements
     * @param range in elements
     * @return the mapped elements, the destructor unmaps
     */
    [[nodiscard]] Mapping<T> map(Range range = {}) const {
        return BaseBuffer::map(bytes(range)).template as<T>();
    }

    /**
     * makes host writes visible to the device
     * @param range in elements
     */
    void flush(Range range = {}) const { BaseBuffer::flush(bytes(range)); }

    /**
     * makes device writes visible to the host
     * @param range in elements
     */
    void invalidate(Range range = {}) const { BaseBuffer::invalidate(bytes(range)); }

    /**
     * copies the data to the buffer
     * @param offset in elements
     */
    void write(std::span<T const> data, size_t offset = 0) const {
        BaseBuffer::write(std::as_bytes(data), offset * sizeof(T));
    }

    /**
     * records a copy from the source buffer
     * @param region in elements
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
     * records a copy from the mip level of the image
     * @param dstOffset in elements
     */
    void copy(
        BaseCmdBuffer const& cmdBuffer,
        Image const& src,
        size_t dstOffset = 0,
        uint32_t mipLevel = 0,
        VkImageLayout layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
    ) const { BaseBuffer::copy(cmdBuffer, src, dstOffset * sizeof(T), mipLevel, layout); }

    [[nodiscard]] constexpr size_t elements() const { return size() / sizeof(T); }

    /**
     * @return gpu address of the first element
     * @pre the preconditions of `BaseBuffer::address()`
     */
    [[nodiscard]] constexpr device_ptr<T> address() const {
        return device_ptr<T>{
            BaseBuffer::address().get()
        };
    }

private:
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
