#pragma once
#include "CthDefaultBuffer.hpp"



namespace cth {
using namespace std;
class Device;


template<typename T>
class Buffer final : public DefaultBuffer {
public:
    /**
     *\brief maps part of the buffer memory
    * \param size in elements
    * \param offset in elements
    * \return mapped memory range
    * \note use map without arguments for whole buffer mapping
    */
    [[nodiscard]] span<T> map(VkDeviceSize size, VkDeviceSize offset);
    /**
    * \return mapped memory of whole buffer
    */
    span<T> map();

    /**
     * \brief stages a device local buffer with a temporary host visible buffer
     * \param buffer_offset in elements
     */
    void stage(span<const T> data, VkDeviceSize buffer_offset = 0) const;

    /**
     * \brief writes to a mapped memory range
     */
    void write(span<const T> data, span<T> mapped_memory, VkDeviceSize mapped_offset = 0) const;
    /**
    * \brief writes to the mapped range of the whole buffer
    * \note CAUTION whole buffer must be mapped first
    */
    void write(span<const T> data, VkDeviceSize mapped_offset = 0) const;

    /**
    * \brief updates non-coherent host visible memory
    * \param size in elements, VK_WHOLE_SIZE -> whole buffer
    * \param offset in elements
     */
    [[nodiscard]] VkResult flush(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0) const override;
    /**
    * \param size in bytes, VK_WHOLE_SIZE -> whole buffer
    * \param offset in bytes
    */
    [[nodiscard]] VkDescriptorBufferInfo descriptorInfo(VkDeviceSize size, VkDeviceSize offset) const override;
    /**
     * \brief 
     * \param size in elements
     * \param offset in elements
     * \return result of vkInvalidateMappedMemoryRanges()
     */
    [[nodiscard]] VkResult invalidate(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0) const override;

    /**
     * \brief copies data from one buffer to another on the gpu
     * \param size in elements, VK_WHOLE_SIZE -> whole buffer
     * \param src_offset in elements
     * \param dst_offset in elements
     */
    void copyFromBuffer(const Buffer<T>& src, VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize src_offset = 0, VkDeviceSize dst_offset = 0) const;
    /**
     * \brief copies data from one buffer to another on the gpu
     * \param size in bytes, VK_WHOLE_SIZE -> whole buffer
     * \param src_offset in bytes
     * \param dst_offset in bytes
     */
    static void copyBuffer(const Buffer<T>* src, const Buffer<T>* dst, VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize src_offset = 0,
        VkDeviceSize dst_offset = 0);

private:
    VkDeviceSize elements;

public:
    Buffer(Device* device, VkDeviceSize element_count, VkBufferUsageFlags usage_flags,
        VkMemoryPropertyFlags memory_property_flags);
    ~Buffer() override = default;

    [[nodiscard]] uint32_t elementCount() const { return elements; }
    [[nodiscard]] static VkDeviceSize elementSize() { return sizeof(T); }


    Buffer(const Buffer& other) = delete;
    Buffer& operator=(const Buffer& other) = delete;
    Buffer(Buffer&& other) = delete;
    Buffer& operator=(Buffer&& other) = delete;
};
}

#include "CthBuffer.inl"
