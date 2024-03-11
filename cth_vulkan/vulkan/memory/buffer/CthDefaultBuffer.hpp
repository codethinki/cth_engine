#pragma once
#include "../descriptors/CthDescriptedResource.hpp"

#include <cth/cth_log.hpp>

#include <span>


namespace cth {
using namespace std;
class Device;

class DefaultBuffer : public DescriptedResource {
public:
    /**
* \brief calculates the min size compatible with the devices minOffsetAlignment
* \param actual_size in bytes
* \param min_offset_alignment in bytes
*/
    static VkDeviceSize calcAlignedSize(VkDeviceSize actual_size, VkDeviceSize min_offset_alignment);
    /**
     *\brief maps part of the buffer memory
    * \param size in bytes
    * \param offset in bytes
    * \return mapped memory range
    * \note use map without arguments for whole buffer mapping
    */
    [[nodiscard]] virtual span<char> default_map(VkDeviceSize size, VkDeviceSize offset);
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
    void default_stage(span<char> data, VkDeviceSize buffer_offset = 0) const;

    /**
     * \brief writes to a mapped memory range
     */
    void default_write(span<char> data, span<char> mapped_memory, VkDeviceSize mapped_offset = 0) const;
    /**
     * \brief writes to the mapped range of the whole buffer
     * \note CAUTION whole buffer must be mapped first
     */
    void default_write(span<char> data, VkDeviceSize mapped_offset = 0) const;

    /**
     * \brief updates non-coherent host visible memory
     * \param size in bytes, VK_WHOLE_SIZE -> whole buffer
     * \param offset in bytes
     */
    [[nodiscard]] virtual VkResult flush(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0) const;

    /**
     * \param size in bytes, VK_WHOLE_SIZE -> whole buffer
     * \param offset in bytes
     */
    [[nodiscard]] Descriptor::descriptor_info_t descriptorInfo(VkDeviceSize size, VkDeviceSize offset) const override;

    /**
    * \brief
    * \param size in elements
    * \param offset in elements
    * \return result of vkInvalidateMappedMemoryRanges()
    */
    [[nodiscard]] virtual VkResult invalidate(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0) const;


    /**
     * \brief copies data from one buffer to another on the gpu
     * \param size in bytes, VK_WHOLE_SIZE -> whole buffer
     * \param src_offset in bytes
     * \param dst_offset in bytes
     */
    virtual void copyFromBuffer(const DefaultBuffer* src, VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize src_offset = 0,
        VkDeviceSize dst_offset = 0) const;

    /**
     * \brief copies data from one buffer to another on the gpu
     * \param size in bytes, VK_WHOLE_SIZE -> whole buffer
     * \param src_offset in bytes
     * \param dst_offset in bytes
     */
    static void copyBuffer(const DefaultBuffer* src, const DefaultBuffer* dst, VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize src_offset = 0,
        VkDeviceSize dst_offset = 0);

private:
    Device* device;
    span<char> mapped{};
    VkBuffer vkBuffer = VK_NULL_HANDLE;
    VkDeviceMemory vkMemory = VK_NULL_HANDLE;

    VkDeviceSize bufferSize;
    VkDeviceSize padding;
    VkBufferUsageFlags vkUsageFlags;
    VkMemoryPropertyFlags vkMemoryPropertyFlags;

public:
    DefaultBuffer(Device* device, VkDeviceSize buffer_size, VkBufferUsageFlags usage_flags,
        VkMemoryPropertyFlags memory_property_flags, VkDeviceSize min_offset_alignment = 1);
    ~DefaultBuffer() override;

    [[nodiscard]] VkBuffer get() const { return vkBuffer; }
    [[nodiscard]] span<char> mappedMemory() const {
        CTH_STABLE_ERR(!mapped.data(), "whole buffer was not mapped");
        return mapped;
    }
    [[nodiscard]] VkBufferUsageFlags usageFlags() const { return vkUsageFlags; }
    [[nodiscard]] VkMemoryPropertyFlags memoryPropertyFlags() const { return vkMemoryPropertyFlags; }
    [[nodiscard]] VkDeviceSize size() const { return bufferSize; }


    DefaultBuffer(const DefaultBuffer& other) = delete;
    DefaultBuffer& operator=(const DefaultBuffer& other) = delete;
    DefaultBuffer(DefaultBuffer&& other) = delete;
    DefaultBuffer& operator=(DefaultBuffer&& other) = delete;
};
}
