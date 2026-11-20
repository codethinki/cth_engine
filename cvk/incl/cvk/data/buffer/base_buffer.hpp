#pragma once
#include "buffer_config.hpp"
#include "device_ptr.hpp"

#include "cvk/base/device_table.hpp"
#include "cvk/data/memory/memory.hpp"
#include "cvk/utility/handle/core_handle.hpp"
#include "cvk/utility/handle/destroy.hpp"
#include "cvk/utility/range.hpp"
#include "cvk/utility/types.hpp"

#include <volk.h>

#include <cth/io/log.hpp>

#include <cstddef>
#include <optional>
#include <span>
#include <tuple>


namespace cvk {
class BaseCmdBuffer;
class Core;
class Image;

/**
 * buffer copy region
 * @details in bytes, in elements for a typed buffer
 */
struct BufferCopy {
    size_t srcOffset = 0;
    size_t dstOffset = 0;

    size_t size = VK_WHOLE_SIZE; ///< (`VK_WHOLE_SIZE` => as much as fits into both buffers)
};

/**
 * `VkBuffer` wrapper
 * @details owns the buffer and its memory
 * @details sizes in bytes
 */
class BaseBuffer {
public:
    using Config = BufferConfig;
    struct State;

    /**
     * base constructor
     */
    BaseBuffer(Core const&, Config const&);

    /**
     * constructs and wraps
     * @details calls @ref BaseBuffer(Core const&, Config const&), `wrap()`
     */
    BaseBuffer(Core const&, Config const&, State const&);

    /**
     * constructs and creates
     * @details calls @ref BaseBuffer(Core const&, Config const&), `create()`
     */
    BaseBuffer(Core const&, Config const&, size_t size);

    /**
     * @note calls `optDestroy()`
     */
    ~BaseBuffer() { optDestroy(); }

    /**
     * wraps the state
     * @details `std::nullopt` `State::memory` -> no own memory, `destroy()` only destroys the buffer
     * @details the usage has `VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT` -> queries `address()`
     * @pre `Core::created()`
     * @pre the buffer is bound to its memory
     * @pre the buffer was created with the usage of the config
     * @post `created()`
     * @note calls `optDestroy()`
     */
    void wrap(State const&);

    /**
     * creates the buffer and its memory
     * @details the usage has `VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT` -> queries `address()`
     * @param size in bytes
     * @pre `Core::created()`
     * @post `created()`
     * @note calls `optDestroy()`
     * @throws cvk::vk_result_exception result of `vmaCreateBuffer()`
     */
    void create(size_t size);

    /**
     * destroys the buffer, then its memory, and forgets
     * @pre `created()`
     * @pre the gpu does not use the buffer
     */
    void destroy();

    /**
     * if `created()` -> calls `destroy()`
     */
    void optDestroy() {
        if(created())
            destroy();
    }

    /**
     * releases ownership of the buffer and its memory, returns the state and forgets
     * @pre `created()`
     */
    State release();

    /**
     * records a copy from the source buffer
     * @param region in bytes
     * @pre `created()` and `src` created
     * @pre `BaseCmdBuffer::recording()`
     * @pre `src` usage has `VK_BUFFER_USAGE_TRANSFER_SRC_BIT`, the usage has `VK_BUFFER_USAGE_TRANSFER_DST_BIT`
     * @pre 0 < copied size, the ranges lie in the buffers
     */
    void copy(BaseCmdBuffer const&, BaseBuffer const& src, BufferCopy region = {}) const;

    /**
     * records a copy from the mip level of the image
     * @param dstOffset in bytes
     * @pre the preconditions of `Image::copyTo()`
     */
    void copy(
        BaseCmdBuffer const&,
        Image const& src,
        size_t dstOffset = 0,
        uint32_t mipLevel = 0,
        VkImageLayout layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
    ) const;

    /**
     * maps the range of the memory
     * @param range in bytes
     * @return the mapped range, its destructor unmaps
     * @pre `memory() != nullptr`
     */
    [[nodiscard]] Mapping<std::byte> map(Range range = {}) const;

    /**
     * makes host writes visible to the device
     * @param range in bytes
     * @pre `memory() != nullptr`, the memory is mapped
     */
    void flush(Range range = {}) const;

    /**
     * makes device writes visible to the host
     * @param range in bytes
     * @pre `memory() != nullptr`, the memory is mapped
     */
    void invalidate(Range range = {}) const;

    /**
     * copies the data to the buffer
     * @param offset in bytes
     * @pre `memory() != nullptr`
     */
    void write(std::span<std::byte const> data, size_t offset = 0) const;

    [[nodiscard]] constexpr bool created() const { return static_cast<bool>(_handle); }
    [[nodiscard]] constexpr VkBuffer get() const { return _handle.get(); }
    [[nodiscard]] constexpr Core const& core() const { return _handle.core(); }
    [[nodiscard]] constexpr Config const& config() const { return _config; }

    /**
     * @return size in bytes
     */
    [[nodiscard]] constexpr size_t size() const { return _size; }

    /**
     * @return `nullptr` -> the buffer does not own its memory
     */
    [[nodiscard]] constexpr Memory const* memory() const {
        return _memoryOpt.has_value() && _memoryOpt->created() ? &*_memoryOpt : nullptr;
    }

    /**
     * @return gpu address of the buffer, the creation or the wrap queried it
     * @pre `created()`
     * @pre the usage has `VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT`
     */
    [[nodiscard]] constexpr device_ptr<std::byte> address() const;

private:
    void forget();

    /**
     * @return `vkGetBufferDeviceAddress()`, `0` -> the usage has no `VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT`
     */
    [[nodiscard]] VkDeviceAddress queryAddress() const;

    /**
     * @pre `memory() != nullptr`
     */
    [[nodiscard]] constexpr Memory const& ownMemory() const;

    // declared before the memory: move assignment destroys the old buffer before its memory
    core_handle<VkBuffer> _handle;
    std::optional<Memory> _memoryOpt = std::nullopt;
    Config _config;
    size_t _size = 0;
    VkDeviceAddress _address = 0;
    ///< `0` -> the usage has no `VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT`

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
    size_t size; ///< in bytes
    std::optional<Memory::State> memory = std::nullopt;
    ///< `std::nullopt` -> the buffer does not own its memory

    /**
     * the raw handles to destroy, in destruction order
     * @return the buffer, then its own memory, `std::nullopt` -> no own memory
     */
    [[nodiscard]] constexpr std::tuple<VkBuffer, std::optional<VmaAllocation>> decompose() && {
        return {
            vkBuffer.get(),
            memory.transform([](Memory::State const& state) { return state.vmaAllocation.get(); })
        };
    }
};
}

namespace cvk {
constexpr Memory const& BaseBuffer::ownMemory() const {
    CTH_CRITICAL(memory() == nullptr, "the buffer must own its memory") {}
    return *_memoryOpt;
}

constexpr device_ptr<std::byte> BaseBuffer::address() const {
    debug_check(*this);
    CTH_CRITICAL(
        (_config.usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) == 0,
        "the usage must have VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, got {}",
        _config.usage
    ) {}
    return device_ptr<std::byte>{_address};
}

constexpr void BaseBuffer::debug_check(BaseBuffer const& buffer) {
    CTH_CRITICAL(!buffer.created(), "buffer must be created") {}
}
}
