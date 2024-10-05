#pragma once
#include "../memory/CthMemory.hpp"
#include "vulkan/utility/cth_constants.hpp"
#include "vulkan/utility/cth_vk_types.hpp"

#include <cth/pointers.hpp>

#include <vulkan/vulkan.h>

#include <span>



namespace cth::vk {
class Memory;
class CmdBuffer;
class Core;
class DestructionQueue;

class BaseBuffer {
public:
    struct State;

    /**
     * @brief base constructor
     */
    BaseBuffer(cth::not_null<Core const*> core, size_t byte_size, VkBufferUsageFlags usage_flags);

    /**
     * @brief constructs and calls @ref wrap(State)
     * @note calls @ref BaseBuffer(cth::not_null<Core const*>, size_t, VkBufferUsageFlags)
     */
    BaseBuffer(cth::not_null<Core const*> core, size_t byte_size, VkBufferUsageFlags usage_flags, State state);

    /**
     * @brief constructs and calls @ref create()
     * @note calls @ref BaseBuffer(cth::not_null<Core const*>, size_t, VkBufferUsageFlags)
     */
    BaseBuffer(cth::not_null<Core const*> core, size_t bytes_size, VkBufferUsageFlags usage_flags, VkMemoryPropertyFlags vk_memory_flags);


    /**
     * @note calls @ref optDestroy()
     */
    virtual ~BaseBuffer() { optDestroy(); }


    virtual void wrap(State state);

    /**
    * @brief creates the image
    * @note calls @ref optDestroy()
    * @note calls @ref Memory::Memory(cth::not_null<Core const*>, VkMemoryPropertyFlags, VkMemoryRequirements const&)
    * @throws cth::vk::result_exception result of @ref vkCreateBuffer()
    * @throws cth::vk::result_exception result of @ref vkBindBufferMemory()
    */
    virtual void create(VkMemoryPropertyFlags vk_memory_flags);

    /**
    * @brief destroys the buffer
    * @attention requires @ref created()
    * @note if Core::destructionQueue() submits to queue
    * @note calls @ref destroy(vk::not_null<VkDevice>, VkBuffer)
    * @note calls @ref Memory::~Memory() if @ref memory()
    */
    virtual void destroy();

    /**
     * @brief if @ref created() calls @ref destroy()
     */
    void optDestroy() { if(created()) destroy(); }


    /**
     * @brief releases ownership and resets
     * @attention requires @ref created()
     */
    State release();

    // ReSharper disable CppHiddenFunction

    /**
     *@brief maps part of the buffer memory
    * @param size in bytes
    * @param offset in bytes
    * @return mapped memory range
    * @attention requires @ref created()
    * @note use @ref map() for whole buffer
    * @note calls @ref Memory::map(size_t, size_t)
    * @note caches result, get with @ref mapped()
    */
    [[nodiscard]] std::span<char> map(size_t size, size_t offset);

    /**
    * @return whole buffer mapped memory range
    * @attention requires @ref created()
    * @note calls @ref Memory::map(size_t, size_t)
    * @note caches result, get with @ref mapped()
    */
    std::span<char> map();

    /**
     * @brief writes to mapped memory
     * @attention requires @ref created()
     * @note requires mapped()
     */
    void write(std::span<char const> data, size_t buffer_offset = 0) const;


    /**
     * @brief updates non-coherent host visible memory
     * @param size in bytes, Constants::WHOLE_SIZE -> whole buffer
     * @param offset in bytes
     * @attention requires @ref created()
     * @note calls Memory::flush(size_t, size_t)
     */
    void flush(size_t size = constants::WHOLE_SIZE, size_t offset = 0) const;

    /**
    * @param size in elements
    * @param offset in elements
    * @attention requires @ref created()
    * @note calls Memory::invalidate(size_t, size_t)
    */
    void invalidate(size_t size = constants::WHOLE_SIZE, size_t offset = 0) const;
    // ReSharper restore CppHiddenFunction

    /**
    * @brief copies buffer data to gpu
    * @param copy_size in bytes (Constants::WHOLE_SIZE => whole buffer)
    * @param src_offset in bytes
    * @param dst_offset in bytes
    * @attention requires @ref created()
     */
    void copy(CmdBuffer const& cmd_buffer, BaseBuffer const& src, size_t copy_size = constants::WHOLE_SIZE, size_t src_offset = 0,
        size_t dst_offset = 0) const;

    /**
    * @brief unmaps all mapped memory ranges
    * @attention requires @ref created()
    * @note calls @ref Memory::unmap()
    */
    void unmap();

    /**
     * @brief stages a device local buffer with a temporary host visible buffer
     * @param dst_offset in bytes
     * @attention requires @ref created()
     * @note calls @ref copy()
     */
    void stage(CmdBuffer const& cmd_buffer, BaseBuffer const& staging_buffer, size_t dst_offset = 0) const;

    /**
    * @param size in bytes, Constants::WHOLE_SIZE -> whole buffer
    * @param offset in bytes
    * @attention requires @ref created()
    */
    [[nodiscard]] virtual VkDescriptorBufferInfo descriptorInfo(size_t size, size_t offset) const;

    /**
    * @brief writes to a mapped memory range
    */
    static void write(std::span<char const> data, std::span<char> mapped_memory);

    /**
    * @brief aligns the buffer size to 16 bc I don't understand buffer alignment
    * @param actual_size in bytes
    */
    static size_t calcAlignedSize(size_t actual_size);

    static void destroy(VkDevice vk_device, VkBuffer vk_buffer);

private:
    /**
     * @brief resets @ref _handle, @ref _memory, @ref _bound, @ref _mapped
     */
    void reset();

    /**
     * @brief creates the VkBuffer
     * @throws cth::vk::result_exception result of @ref vkCreateBuffer()
     */
    void createBuffer();

    /**
     * @brief creates the memory
     * @note calls @ref Memory::Memory(cth::not_null<Core const*>, VkMemoryPropertyFlags)
     * @note calls @ref Memory::create(VkMemoryRequirements const&)
     */
    void createMemory(VkMemoryPropertyFlags vk_memory_properties);


    /**
    * @brief binds buffer to memory
    * @attention @ref Memory::created() required
    * @throws cth::vk::result_exception result of @ref vkBindBufferMemory()
    */
    void bind();

    cth::not_null<Core const*> _core;
    size_t _size;
    VkBufferUsageFlags _usage;


    move_ptr<VkBuffer_T> _handle = VK_NULL_HANDLE;
    std::unique_ptr<Memory> _memory = nullptr;
    bool _bound = false;
    std::span<char> _mapped;

public:
    [[nodiscard]] auto get() const { return _handle.get(); }
    [[nodiscard]] Memory* memory() const;
    [[nodiscard]] auto bound() const { return _bound; }
    [[nodiscard]] auto mapped() const { return _mapped; }
    [[nodiscard]] auto usageFlags() const { return _usage; }
    [[nodiscard]] auto size() const { return _size; }
    [[nodiscard]] bool created() const { return _handle != VK_NULL_HANDLE; }

    BaseBuffer(BaseBuffer const& other) = delete;
    BaseBuffer& operator=(BaseBuffer const& other) = delete;
    BaseBuffer(BaseBuffer&& other) noexcept = default;
    BaseBuffer& operator=(BaseBuffer&& other) noexcept = default;

    static void debug_check(BaseBuffer const* buffer);
    static void debug_check_state(State const& state);
};
} // namespace cth

//State

namespace cth::vk {
struct BaseBuffer::State {
    vk::not_null<VkBuffer> vkBuffer;
    /**
     * @note may be nullptr if not @ref bound
     */
    std::unique_ptr<Memory> memory;
    /**
     * @note if bound but @ref memory is nullptr -> memory unknown
     */
    bool bound;
    /**
     * @note may be empty
     * @attention must not specify an offset into the buffer
     */
    std::span<char> mapped{};
};
}

//debug checks

namespace cth::vk {
inline void BaseBuffer::debug_check(BaseBuffer const* buffer) {
    CTH_CRITICAL(buffer == nullptr, "buffer must not be nullptr") {}
    CTH_CRITICAL(buffer->_handle == VK_NULL_HANDLE, "buffer must be a valid handle") {}
}
inline void BaseBuffer::debug_check_state(State const& state) {
    if(state.memory) Memory::debug_check(state.memory.get());
    else { CTH_CRITICAL(!state.bound, "state must have created memory or be bound") {} }
}
}
