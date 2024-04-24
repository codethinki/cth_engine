#pragma once
#include "CthBasicBuffer.hpp"



namespace cth {
class DeletionQueue;
}

namespace cth {
class Device;


//TEMP left off here. recreate the hierarchy of the image class with BaseImage and Image in the buffer class.
template<typename T>
class Buffer final : public BasicBuffer {
public:
    Buffer(Device* device, DeletionQueue* deletion_queue, size_t element_count, VkBufferUsageFlags usage_flags,
        VkMemoryPropertyFlags memory_property_flags);
    ~Buffer() override;

    /**
    * \brief creates the buffer
    * \note previous buffer will be destroyed
    */
    void create() override;

    /**
    * \brief submits buffer & memory to cached deletion queues and resets the object
    * \param deletion_queue != nullptr => submits to new deletion queue
    * \note new deletion queue will be cached
    */
    void destroy(DeletionQueue* deletion_queue = nullptr) override;

    /**
    * \param new_memory must not be allocated or nullptr
    * \note destroys current memory
    */
    void setMemory(BasicMemory* new_memory) override;

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
     * \param dst_offset in elements
     */
    void stage(const CmdBuffer* cmd_buffer, const Buffer<T>* staging_buffer, size_t dst_offset = 0) const;

    /**
     * \brief writes to a mapped memory range
     */
    static void write(span<const T> data, span<T> mapped_memory);
    /**
    * \brief writes to the mapped range of the whole buffer
    * \note CAUTION whole buffer must be mapped first
    */
    void write(span<const T> data, size_t mapped_offset = 0) const;

    /**
    * \brief copies buffer data on the gpu
    * \param cmd_buffer
    * \param copy_size in elements (VK_WHOLE_SIZE => whole buffer)
    * \param src_offset in elements
    * \param dst_offset in elements
    */
    void copy(const CmdBuffer* cmd_buffer, const Buffer<T>* src, size_t copy_size = VK_WHOLE_SIZE, size_t src_offset = 0, size_t dst_offset = 0) const;

    /**
    * \brief updates non-coherent host visible memory
    * \param size in elements, VK_WHOLE_SIZE -> whole buffer
    * \param offset in elements
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
    * \param size in elements, VK_WHOLE_SIZE -> whole buffer
    * \param offset in elements
    */
    [[nodiscard]] VkDescriptorBufferInfo descriptorInfo(size_t size, size_t offset) const override;


private:
    void destroyMemory(DeletionQueue* deletion_queue = nullptr);

    size_t _elements;
    DeletionQueue* deletionQueue;
public:
    [[nodiscard]] uint32_t elements() const { return _elements; }

    Buffer(const Buffer& other) = delete;
    Buffer& operator=(const Buffer& other) = delete;
    Buffer(Buffer&& other) = delete;
    Buffer& operator=(Buffer&& other) = delete;
};

} // namespace cth

#include "CthBuffer.inl"
