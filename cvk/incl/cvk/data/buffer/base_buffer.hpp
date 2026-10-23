#pragma once
#include "buffer_config.hpp"

#include "cvk/base/device_table.hpp"
#include "cvk/data/memory/memory.hpp"
#include "cvk/utility/handle/core_handle.hpp"
#include "cvk/utility/range.hpp"
#include "cvk/utility/types.hpp"

#include <volk.h>

#include <cth/io/log.hpp>

#include <cstddef>
#include <optional>
#include <span>


namespace cvk {
class BaseCmdBuffer;
class Core;
class Image;

/**
 * @brief offsets and size of a buffer copy
 * @details in bytes, in elements for a typed buffer
 */
struct BufferCopy {
    size_t srcOffset = 0;
    size_t dstOffset = 0;

    /**
     * @brief (VK_WHOLE_SIZE => as much as fits into both buffers)
     */
    size_t size = VK_WHOLE_SIZE;
};

/**
 * @brief VkBuffer wrapper, owns the buffer and its memory, sizes in bytes
 * @details @ref create() creates both with vmaCreateBuffer(),
 *  map, flush, invalidate and write forward to the memory,
 *  @ref Buffer adds an element type
 */
class BaseBuffer {
public:
    using vk_handle_t = VkBuffer;
    using Config = BufferConfig;
    struct State;

    /**
     * @brief base constructor
     * @pre @ref Core::created()
     */
    BaseBuffer(Core const&, Config const&);

    /**
     * @brief constructs and wraps
     * @details calls:
     * - @ref BaseBuffer(Core const&, Config const&)
     * - @ref wrap()
     */
    BaseBuffer(Core const&, Config const&, State const&);

    /**
     * @brief constructs and creates
     * @details calls:
     * - @ref BaseBuffer(Core const&, Config const&)
     * - @ref create()
     */
    BaseBuffer(Core const&, Config const&, size_t size);

    /**
     * @note calls @ref optDestroy()
     */
    ~BaseBuffer() { optDestroy(); }

    /**
     * @brief wraps the state
     * @details std::nullopt @ref State::memory -> no own memory, @ref destroy() only destroys the buffer
     * @pre the buffer is bound to its memory
     * @pre the buffer was created with the usage of the config
     * @post @ref created()
     * @note calls @ref optDestroy()
     */
    void wrap(State const&);

    /**
     * @brief creates the buffer and its memory
     * @param size in bytes
     * @post @ref created()
     * @note calls @ref optDestroy()
     * @throws cvk::vk_result_exception result of vmaCreateBuffer()
     */
    void create(size_t size);

    /**
     * @brief destroys the buffer, then its memory, and forgets
     * @pre @ref created()
     * @pre the gpu does not use the buffer
     */
    void destroy();

    /**
     * @brief if @ref created() -> calls @ref destroy()
     */
    void optDestroy() { if(created()) destroy(); }

    /**
     * @brief releases ownership of the buffer and its memory, returns the state and forgets
     * @pre @ref created()
     */
    State release();

    /**
     * @brief records a copy from the source buffer
     * @param region in bytes
     * @pre @ref created() and src created
     * @pre @ref BaseCmdBuffer::recording()
     * @pre src usage has VK_BUFFER_USAGE_TRANSFER_SRC_BIT, the usage has VK_BUFFER_USAGE_TRANSFER_DST_BIT
     * @pre 0 < copied size, the ranges lie in the buffers
     */
    void copy(BaseCmdBuffer const&, BaseBuffer const& src, BufferCopy region = {}) const;

    /**
     * @brief records a copy from the mip level of the image
     * @param dstOffset in bytes
     * @details calls @ref Image::copyTo()
     */
    void copy(
        BaseCmdBuffer const&,
        Image const& src,
        size_t dstOffset = 0,
        uint32_t mipLevel = 0,
        VkImageLayout layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
    ) const;

    /**
     * @brief maps the range of the memory
     * @param range in bytes
     * @return the mapped range, its destructor unmaps
     * @pre @ref memory() != nullptr
     * @details calls @ref Memory::map()
     */
    [[nodiscard]] Mapping<std::byte> map(Range range = {}) const;

    /**
     * @brief makes host writes visible to the device
     * @param range in bytes
     * @pre @ref memory() != nullptr, the memory is mapped
     * @details calls @ref Memory::flush()
     */
    void flush(Range range = {}) const;

    /**
     * @brief makes device writes visible to the host
     * @param range in bytes
     * @pre @ref memory() != nullptr, the memory is mapped
     * @details calls @ref Memory::invalidate()
     */
    void invalidate(Range range = {}) const;

    /**
     * @brief copies the data to the buffer
     * @param offset in bytes
     * @pre @ref memory() != nullptr
     * @details calls @ref Memory::write()
     */
    void write(std::span<std::byte const> data, size_t offset = 0) const;

    static void destroy(DeviceTable table, VkBuffer);

    [[nodiscard]] constexpr bool created() const { return static_cast<bool>(_handle); }
    [[nodiscard]] constexpr vk_handle_t get() const { return _handle.get(); }
    [[nodiscard]] constexpr Core const& core() const { return _handle.core(); }
    [[nodiscard]] constexpr Config const& config() const { return _config; }

    /**
     * @return size in bytes
     */
    [[nodiscard]] constexpr size_t size() const { return _size; }

    /**
     * @return nullptr -> the buffer does not own its memory
     */
    [[nodiscard]] constexpr Memory const* memory() const {
        return _memory.has_value() && _memory->created() ? &*_memory : nullptr;
    }

private:
    void forget();

    /**
     * @pre @ref memory() != nullptr
     */
    [[nodiscard]] constexpr Memory const& ownMemory() const;

    // declared before the memory: move assignment destroys the old buffer before its memory
    core_handle<BaseBuffer> _handle;
    std::optional<Memory> _memory = std::nullopt;
    Config _config;
    size_t _size = 0;

public:
    BaseBuffer(BaseBuffer const& other) = delete;
    BaseBuffer(BaseBuffer&& other) noexcept = default;
    BaseBuffer& operator=(BaseBuffer const& other) = delete;
    BaseBuffer& operator=(BaseBuffer&& other) noexcept = default;

    static constexpr void debug_check(BaseBuffer const&);
};
}

namespace cvk {
struct BaseBuffer::State {
    vk_not_null<VkBuffer> vkBuffer;
    /**
     * @brief in bytes
     */
    size_t size;
    /**
     * @brief std::nullopt -> the buffer does not own its memory
     */
    std::optional<Memory::State> memory = std::nullopt;
};
}

namespace cvk {
constexpr Memory const& BaseBuffer::ownMemory() const {
    CTH_CRITICAL(memory() == nullptr, "the buffer must own its memory") {}
    return *_memory;
}

constexpr void BaseBuffer::debug_check(BaseBuffer const& buffer) {
    CTH_CRITICAL(!buffer.created(), "buffer must be created") {}
}
}
