#pragma once
#include "../memory/CthBasicMemory.hpp"
#include "vulkan/utility/CthConstants.hpp"

#include<cth/cth_pointer.hpp>
#include <cth/io/cth_log.hpp>

#include <vulkan/vulkan.h>

#include <span>



namespace cth::vk {
class BasicMemory;
class CmdBuffer;
class BasicCore;
class DeletionQueue;
class Memory;

class BasicBuffer {
public:
    struct State;

    /**
     * @param buffer_size in bytes
     */
    BasicBuffer(const BasicCore* core, size_t buffer_size, VkBufferUsageFlags usage_flags);
    BasicBuffer(const BasicCore* core, size_t buffer_size, VkBufferUsageFlags usage_flags, VkBuffer vk_buffer, State state);

    virtual ~BasicBuffer() = default;


    virtual void wrap(VkBuffer vk_buffer, const State& state);

    /**
    * @brief creates the image
    * @note buffer must not be a valid handle
    */
    virtual void create();

    /**
    * @brief allocates buffer memory
    * @param new_memory must not be allocated or nullptr
    */
    void alloc(BasicMemory* new_memory);
    /**
    * @brief allocates buffer memory
    * @note memory must not be allocated
    */
    void alloc() const;

    /**
    * @brief binds buffer to new_memory and replaces the old one
    * @param new_memory must be allocated
    */
    void bind(BasicMemory* new_memory);
    /**
    * binds buffer to memory
    * @note memory must be allocated
    */
    void bind() const;


    /**
    * @brief destroys the buffer
    * @param deletion_queue != nullptr => submit to deletion queue
    * @note memory will not be reset
    */
    virtual void destroy(DeletionQueue* deletion_queue);

    /**
     * @brief resets the buffer and its state
     * @note does not destroy buffer or memory
     */
    virtual void reset();

    [[nodiscard]] BasicMemory* releaseMemory();

    /**
     *@brief maps part of the buffer memory
    * @param size in bytes
    * @param offset in bytes
    * @return mapped memory range
    * @note use map without arguments for whole buffer mapping
    */
    [[nodiscard]] std::span<char> map(size_t size, size_t offset);
    /**
    * @return whole buffer mapped memory range
    */
    std::span<char> map();

    /**
     *@brief unmaps all mapped memory ranges
     */
    void unmap();

    /**
     * @brief stages a device local buffer with a temporary host visible buffer
     * @param dst_offset in bytes
     */
    void stage(const CmdBuffer& cmd_buffer, const BasicBuffer& staging_buffer, size_t dst_offset = 0) const;

    /**
     * @brief writes to the mapped range of the whole buffer
     * @note whole buffer must be mapped first
     * @note not virtual
     */
    void write(std::span<const char> data, size_t buffer_offset = 0) const;


    /**
    * @brief copies buffer data on the gpu
    * @param cmd_buffer
    * @param copy_size in bytes (Constants::WHOLE_SIZE => whole buffer)
    * @param src_offset in bytes
    * @param dst_offset in bytes
     */
    void copy(const CmdBuffer& cmd_buffer, const BasicBuffer& src, size_t copy_size = constant::WHOLE_SIZE, size_t src_offset = 0,
        size_t dst_offset = 0) const;


    /**
     * @brief updates non-coherent host visible memory
     * @param size in bytes, Constants::WHOLE_SIZE -> whole buffer
     * @param offset in bytes
     */
    [[nodiscard]] VkResult flush(size_t size = constant::WHOLE_SIZE, size_t offset = 0) const;

    /**
    * @param size in elements
    * @param offset in elements
    * @return result of vkInvalidateMappedMemoryRanges()
    */
    [[nodiscard]] VkResult invalidate(size_t size = constant::WHOLE_SIZE, size_t offset = 0) const;


    /**
    * @param size in bytes, Constants::WHOLE_SIZE -> whole buffer
    * @param offset in bytes
    */
    [[nodiscard]] virtual VkDescriptorBufferInfo descriptorInfo(size_t size, size_t offset) const;

    /**
    * @brief writes to a mapped memory range
    */
    static void write(std::span<const char> data, std::span<char> mapped_memory);

    static void destroy(VkDevice vk_device, VkBuffer vk_buffer);

    /**
    * @brief aligns the buffer size to 16 bc I don't understand buffer alignment
    * @param actual_size in bytes
    */
    static size_t calcAlignedSize(size_t actual_size);

    struct State {
        move_ptr<BasicMemory> memory = nullptr;
        bool bound = memory != nullptr;
        std::span<char> mapped{}; //must not specify an offset into the buffer

        static State Default() { return State{}; }

    private:
        void reset();
        friend BasicBuffer;
    };

protected:
    /**
    * @brief destroys the memory
    * @param deletion_queue deletion_queue != nullptr => submit to deletion queue
    * @note the memory destruction happens immediately only the free() is queued
    */
    void destroyMemory(DeletionQueue* deletion_queue = nullptr);

    /**
    * @param new_memory must not be allocated, nullptr or current memory
    * @note does not free current memory
    */
    virtual void setMemory(BasicMemory* new_memory);

    const BasicCore* _core;
    size_t _size;
    VkBufferUsageFlags _usage;


    State _state = State::Default();

private:
    void init() const;

    move_ptr<VkBuffer_T> _handle = VK_NULL_HANDLE;

public:
    [[nodiscard]] auto get() const { return _handle.get(); }
    [[nodiscard]] BasicMemory* memory() const;
    [[nodiscard]] auto bound() const { return _state.bound; }
    [[nodiscard]] auto mapped() const { return _state.mapped; }
    [[nodiscard]] auto usageFlags() const { return _usage; }
    [[nodiscard]] auto size() const { return _size; }
    [[nodiscard]] bool allocated() const;
    [[nodiscard]] auto state() const { return _state; }

    BasicBuffer(const BasicBuffer& other) = default;
    BasicBuffer(BasicBuffer&& other) = default;
    BasicBuffer& operator=(const BasicBuffer& other) = default;
    BasicBuffer& operator=(BasicBuffer&& other) = default;

#ifdef CONSTANT_DEBUG_MODE
    static void debug_check(const BasicBuffer* buffer);
    static void debug_check_leak(const BasicBuffer* buffer);
    static void debug_check_memory_leak(const BasicBuffer* buffer);
    static void debug_check_not_bound(const BasicBuffer* buffer);

#define DEBUG_CHECK_BUFFER(buffer_ptr) BasicBuffer::debug_check(buffer_ptr)
#define DEBUG_CHECK_BUFFER_LEAK(buffer_ptr) BasicBuffer::debug_check_leak(buffer_ptr)
#define DEBUG_CHECK_BUFFER_MEMORY_LEAK(buffer_ptr) BasicBuffer::debug_check_memory_leak(buffer_ptr)
#define DEBUG_CHECK_BUFFER_NOT_BOUND(buffer_ptr) BasicBuffer::debug_check_not_bound(buffer_ptr)
#else
#define DEBUG_CHECK_BUFFER(buffer_ptr) ((void)0)
#define DEBUG_CHECK_BUFFER_LEAK(buffer_ptr)  ((void)0)
#define DEBUG_CHECK_BUFFER_NOT_BOUND(buffer_ptr)  ((void)0)
#endif
};
} // namespace cth
