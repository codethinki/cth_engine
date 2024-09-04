#pragma once
#include "CthBasicBuffer.hpp"



namespace cth::vk {
class DestructionQueue;
}

namespace cth::vk {
class Device;


template<typename T>
class Buffer final : public BasicBuffer {
public:
    Buffer(cth::not_null<BasicCore const*> core, size_t element_count, VkBufferUsageFlags usage_flags,
        VkMemoryPropertyFlags memory_property_flags);
    ~Buffer() override;

    void wrap(VkBuffer vk_buffer, State const& state) override;

    /**
    * @brief creates the buffer
    * @note previous buffer will be destroyed
    */
    void create() override;

    /**
    * @brief submits buffer & memory to cached deletion queues and resets the object
    * @param destruction_queue != nullptr => submits to new deletion queue
    * @note new deletion queue will be cached
    */
    void destroy(DestructionQueue* destruction_queue = nullptr) override;

    /**
     *@brief maps part of the buffer memory
    * @param size in elements
    * @param offset in elements
    * @return mapped memory range
    * @note use map without arguments for whole buffer mapping
    */
    [[nodiscard]] std::span<T> map(size_t size, size_t offset);
    /**
    * @return mapped memory of whole buffer
    */
    std::span<T> map();

    /**
     * @brief stages a device local buffer with a temporary host visible buffer
     * @param dst_offset in elements
     */
    void stage(CmdBuffer const& cmd_buffer, Buffer<T> const& staging_buffer, size_t dst_offset = 0) const;

    /**
     * @brief writes to a mapped memory range
     */
    static void write(std::span<T const> data, std::span<T> mapped_memory);
    /**
    * @brief writes to the mapped range of the whole buffer
    * @note CAUTION whole buffer must be mapped first
    */
    void write(std::span<T const> data, size_t mapped_offset = 0) const;

    /**
    * @brief copies buffer data on the gpu
    * @param cmd_buffer
    * @param copy_size in elements (Constants::WHOLE_SIZE => whole buffer)
    * @param src_offset in elements
    * @param dst_offset in elements
    */
    void copy(CmdBuffer const& cmd_buffer, Buffer<T> const& src, size_t copy_size = constants::WHOLE_SIZE, size_t src_offset = 0,
        size_t dst_offset = 0) const;

    /**
    * @brief updates non-coherent host visible memory
    * @param size in elements, Constants::WHOLE_SIZE -> whole buffer
    * @param offset in elements
     */
    [[nodiscard]] VkResult flush(size_t size = constants::WHOLE_SIZE, size_t offset = 0) const;

    /**
     * @param size in elements
     * @param offset in elements
     * @return result of @ref vkInvalidateMappedMemoryRanges()
     */
    [[nodiscard]] VkResult invalidate(size_t size = constants::WHOLE_SIZE, size_t offset = 0) const;

    /**
    * @param size in elements, Constants::WHOLE_SIZE -> whole buffer
    * @param offset in elements
    */
    [[nodiscard]] VkDescriptorBufferInfo descriptorInfo(size_t size, size_t offset) const override;

private:
    /**
    * @param new_memory must not be allocated or nullptr
    * @note destroys current memory
    */
    void setMemory(Memory* new_memory) override;

    size_t _elements;
    DestructionQueue* _destructionQueue; //TEMP remove this

public:
    [[nodiscard]] uint32_t elements() const { return _elements; }

    Buffer(Buffer const& other) = delete;
    Buffer(Buffer&& other) = default;
    Buffer& operator=(Buffer const& other) = delete;
    Buffer& operator=(Buffer&& other) = default;
};

} // namespace cth

#include "CthBuffer.inl"
