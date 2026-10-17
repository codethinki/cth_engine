#pragma once
#include "buffer_config.hpp"

#include "cvk/base/device_table.hpp"
#include "cvk/data/memory/memory.hpp"
#include "cvk/utility/core_handle.hpp"
#include "cvk/utility/types.hpp"

#include <volk.h>

#include <cth/io/log.hpp>

#include <optional>


namespace cvk {
class Core;

/**
 * @brief VkBuffer wrapper, owns the buffer and its memory, sizes in bytes
 * @details @ref create() creates both with vmaCreateBuffer(),
 *  map, flush and write through @ref memory(),
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
     * @details std::nullopt @ref State::memory -> @ref destroy() only destroys the buffer
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
     * @param size in bytes (VK_WHOLE_SIZE => to the end)
     * @param offset in bytes
     * @pre @ref created()
     * @pre offset + size <= @ref size()
     */
    [[nodiscard]] constexpr VkDescriptorBufferInfo descriptorInfo(size_t size = VK_WHOLE_SIZE, size_t offset = 0) const;

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
     * @return the memory of the buffer, not created -> the buffer does not own its memory
     */
    [[nodiscard]] constexpr Memory const& memory() const { return _memory; }

private:
    void forget();

    // declared before the memory: move assignment destroys the old buffer before its memory
    core_handle<BaseBuffer> _handle;
    Memory _memory;
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
constexpr VkDescriptorBufferInfo BaseBuffer::descriptorInfo(size_t size, size_t offset) const {
    debug_check(*this);
    CTH_CRITICAL(offset > _size, "offset must lie in the buffer") {}
    CTH_CRITICAL(size != VK_WHOLE_SIZE && size > _size - offset, "range must lie in the buffer") {}

    return VkDescriptorBufferInfo{
        .buffer = get(),
        .offset = offset,
        .range = size == VK_WHOLE_SIZE ? _size - offset : size,
    };
}
}

namespace cvk {
constexpr void BaseBuffer::debug_check(BaseBuffer const& buffer) {
    CTH_CRITICAL(!buffer.created(), "buffer must be created") {}
}
}
