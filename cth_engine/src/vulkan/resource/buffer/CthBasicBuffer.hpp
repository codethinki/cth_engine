#pragma once
#include <cth/cth_log.hpp>

#include <vulkan/vulkan.h>

#include <span>



namespace cth {
class Device;
class BasicMemory;
class Memory;
class CmdBuffer;

using std::span;
using std::unique_ptr;

class BasicBuffer {
public:
    /**
     * \param buffer_size in bytes
     */
    BasicBuffer(Device* device, size_t buffer_size, VkBufferUsageFlags usage_flags, Memory* memory);
    virtual ~BasicBuffer() = default;

    void create();
    void alloc() const;
    void bind() const;
    /**
    * \brief aligns the buffer size to 16 bc I don't understand buffer alignment
    * \param actual_size in bytes
    */
    static size_t calcAlignedSize(size_t actual_size);
    /**
     *\brief maps part of the buffer memory
    * \param size in bytes
    * \param offset in bytes
    * \return mapped memory range
    * \note use map without arguments for whole buffer mapping
    */
    [[nodiscard]] span<char> map(size_t size, size_t offset);
    /**
    * \return whole buffer mapped memory range
    */
    span<char> map();


    /**
     *\brief unmaps all mapped memory ranges
     */
    void unmap();

    /**
     * \brief stages a device local buffer with a temporary host visible buffer
     * \param dst_offset in bytes
     */
    void stage(const CmdBuffer* cmd_buffer, const BasicBuffer* staging_buffer, size_t dst_offset = 0) const;

    /**
     * \brief writes to a mapped memory range
     */
    static void write(span<const char> data, span<char> mapped_memory);

    /**
     * \brief writes to the mapped range of the whole buffer
     * \note whole buffer must be mapped first
     * \note not virtual
     */
    void write(span<const char> data, size_t buffer_offset = 0) const;


    /**
    * \brief copies buffer data on the gpu
    * \param cmd_buffer
    * \param copy_size in bytes (VK_WHOLE_SIZE => whole buffer)
    * \param src_offset in bytes
    * \param dst_offset in bytes
     */
    void copy(const CmdBuffer* cmd_buffer, const BasicBuffer* src, size_t copy_size = VK_WHOLE_SIZE, size_t src_offset = 0,
        size_t dst_offset = 0) const;


    /**
     * \brief updates non-coherent host visible memory
     * \param size in bytes, VK_WHOLE_SIZE -> whole buffer
     * \param offset in bytes
     */
    [[nodiscard]] VkResult flush(size_t size = VK_WHOLE_SIZE, size_t offset = 0) const;

    /**
    * \brief
    * \param size in elements
    * \param offset in elements
    * \return result of vkInvalidateMappedMemoryRanges()
    */
    [[nodiscard]] VkResult invalidate(size_t size = VK_WHOLE_SIZE, size_t offset = 0) const;

    /**
    * \param size in bytes, VK_WHOLE_SIZE -> whole buffer
    * \param offset in bytes
    */
    [[nodiscard]] virtual VkDescriptorBufferInfo descriptorInfo(size_t size, size_t offset) const;

    virtual void destroy();

    static void destroy(const Device* device, VkBuffer vk_buffer);

protected:
    void reset();

private:
    Device* device;
    span<char> mapped{};
    VkBuffer vkBuffer = VK_NULL_HANDLE;

    Memory* _memory;

    size_t _size;
    VkBufferUsageFlags _usage;

public:
    [[nodiscard]] VkBuffer get() const { return vkBuffer; }
    [[nodiscard]] span<char> mappedMemory() const {
        CTH_STABLE_ERR(!mapped.data(), "whole buffer was not mapped");
        return mapped;
    }
    [[nodiscard]] Memory* memory() const { return _memory; }
    [[nodiscard]] VkBufferUsageFlags usageFlags() const { return _usage; }
    [[nodiscard]] size_t size() const { return _size; }


    BasicBuffer(const BasicBuffer& other) = delete;
    BasicBuffer& operator=(const BasicBuffer& other) = delete;
    BasicBuffer(BasicBuffer&& other) = delete;
    BasicBuffer& operator=(BasicBuffer&& other) = delete;
};
}
