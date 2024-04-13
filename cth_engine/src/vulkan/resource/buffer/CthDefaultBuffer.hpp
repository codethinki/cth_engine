#pragma once

#include <cth/cth_log.hpp>

#include <vulkan/vulkan.h>

#include <span>

namespace cth {
using namespace std;
class Device;

class DefaultBuffer {
public:
    DefaultBuffer(Device* device, size_t buffer_size, VkBufferUsageFlags usage_flags,
        VkMemoryPropertyFlags memory_property_flags);
    virtual ~DefaultBuffer();

    /**
    * \brief aligns the buffer size to 16 bc i dont understand buffer alignment
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
    [[nodiscard]] virtual span<char> default_map(size_t size, size_t offset);
    /**
    * \return whole buffer mapped memory range
    */
    span<char> default_map();


    /**
     *\brief unmaps all mapped memory ranges
     */
    void unmap();

    /**
     * \brief stages a device local buffer with a temporary host visible buffer
     * \param buffer_offset in bytes
     */
    void default_stage(span<const char> data, size_t buffer_offset = 0) const;

    /**
     * \brief writes to a mapped memory range
     */
    static void default_write(span<const char> data, span<char> mapped_memory);
    /**
     * \brief writes to the mapped range of the whole buffer
     * \note CAUTION whole buffer must be mapped first
     */
    void default_write(span<const char> data, size_t buffer_offset = 0) const;


    /**
    * \brief copies buffer data on the gpu
    * \param copy_size in bytes (VK_WHOLE_SIZE => whole buffer)
    * \param src_offset in bytes
    * \param dst_offset in bytes
     */
    virtual void copy(DefaultBuffer* src, size_t copy_size = VK_WHOLE_SIZE, size_t src_offset = 0, size_t dst_offset = 0) const;


    /**
     * \brief updates non-coherent host visible memory
     * \param size in bytes, VK_WHOLE_SIZE -> whole buffer
     * \param offset in bytes
     */
    [[nodiscard]] virtual VkResult flush(size_t size = VK_WHOLE_SIZE, size_t offset = 0) const;

    /**
     * \param size in bytes, VK_WHOLE_SIZE -> whole buffer
     * \param offset in bytes
     */
    [[nodiscard]] virtual VkDescriptorBufferInfo descriptorInfo(size_t size, size_t offset) const;

    /**
    * \brief
    * \param size in elements
    * \param offset in elements
    * \return result of vkInvalidateMappedMemoryRanges()
    */
    [[nodiscard]] virtual VkResult invalidate(size_t size = VK_WHOLE_SIZE, size_t offset = 0) const;



    /**
     * \brief copies data from one buffer to another on the gpu
     * \param size in bytes, VK_WHOLE_SIZE -> whole buffer
     * \param src_offset in bytes
     * \param dst_offset in bytes
     */
    static void copyBuffer(const DefaultBuffer* src, const DefaultBuffer* dst, size_t size = VK_WHOLE_SIZE, size_t src_offset = 0,
        size_t dst_offset = 0);

private:
    void create();
    void alloc();

    Device* device;
    span<char> mapped{};
    VkBuffer vkBuffer = VK_NULL_HANDLE;
    VkDeviceMemory vkMemory = VK_NULL_HANDLE;

    size_t _size;
    size_t padding;
    VkBufferUsageFlags _usage;
    VkMemoryPropertyFlags _memoryProperties;

public:
    [[nodiscard]] VkBuffer get() const { return vkBuffer; }
    [[nodiscard]] span<const char> mappedMemory() const {
        CTH_STABLE_ERR(!mapped.data(), "whole buffer was not mapped");
        return mapped;
    }
    [[nodiscard]] VkBufferUsageFlags usageFlags() const { return _usage; }
    [[nodiscard]] VkMemoryPropertyFlags memoryPropertyFlags() const { return _memoryProperties; }
    [[nodiscard]] size_t size() const { return _size; }


    DefaultBuffer(const DefaultBuffer& other) = delete;
    DefaultBuffer& operator=(const DefaultBuffer& other) = delete;
    DefaultBuffer(DefaultBuffer&& other) = delete;
    DefaultBuffer& operator=(DefaultBuffer&& other) = delete;
};
}
