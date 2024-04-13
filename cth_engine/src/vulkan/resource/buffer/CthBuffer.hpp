#pragma once
#include "CthDefaultBuffer.hpp"



namespace cth {
using namespace std;
class Device;


template<typename T>
class Buffer final : public DefaultBuffer {
public:
    Buffer(Device* device, size_t element_count, VkBufferUsageFlags usage_flags,
        VkMemoryPropertyFlags memory_property_flags);
    ~Buffer() override = default;

    /**
     *\brief maps part of the buffer memory
    * \param size in elements
    * \param offset in elements
    * \return mapped memory range
    * \note use map without arguments for whole buffer mapping
    */
    [[nodiscard]] span<T> map(size_t size, size_t offset);
    /**
    * \return mapped memory of whole buffer
    */
    span<T> map();

    /**
     * \brief stages a device local buffer with a temporary host visible buffer
     * \param buffer_offset in elements
     */
    void stage(span<const T> data, size_t buffer_offset = 0) const;

    /**
     * \brief writes to a mapped memory range
     */
    void write(span<const T> data, span<T> mapped_memory, size_t mapped_offset = 0) const;
    /**
    * \brief writes to the mapped range of the whole buffer
    * \note CAUTION whole buffer must be mapped first
    */
    void write(span<const T> data, size_t mapped_offset = 0) const;

    /**
    * \brief copies buffer data on the gpu
    * \param copy_size in elements (VK_WHOLE_SIZE => whole buffer)
    * \param src_offset in elements
    * \param dst_offset in elements
    */
    void copy(Buffer<T>* src, size_t copy_size = VK_WHOLE_SIZE, size_t src_offset = 0, size_t dst_offset = 0) const;

    /**
    * \brief updates non-coherent host visible memory
    * \param size in elements, VK_WHOLE_SIZE -> whole buffer
    * \param offset in elements
     */
    [[nodiscard]] VkResult flush(size_t size = VK_WHOLE_SIZE, size_t offset = 0) const override;
    /**
    * \param size in bytes, VK_WHOLE_SIZE -> whole buffer
    * \param offset in bytes
    */
    [[nodiscard]] VkDescriptorBufferInfo descriptorInfo(size_t size, size_t offset) const override;
    /**
     * \brief 
     * \param size in elements
     * \param offset in elements
     * \return result of vkInvalidateMappedMemoryRanges()
     */
    [[nodiscard]] VkResult invalidate(size_t size = VK_WHOLE_SIZE, size_t offset = 0) const override;

private:
    size_t _elements;

public:
    [[nodiscard]] uint32_t elements() const { return _elements; }


    Buffer(const Buffer& other) = delete;
    Buffer& operator=(const Buffer& other) = delete;
    Buffer(Buffer&& other) = delete;
    Buffer& operator=(Buffer&& other) = delete;
};
}

#include "CthBuffer.inl"
