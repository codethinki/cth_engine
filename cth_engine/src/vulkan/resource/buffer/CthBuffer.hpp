#pragma once
#include "CthBasicBuffer.hpp"



namespace cth::vk {
template<typename T>
class Buffer final : public BaseBuffer {
public:
    /**
     * @brief base constructor
     * @param element_count of T
     * @note calls @ref BaseBuffer(cth::not_null<BasicCore const*>, size_t, VkBufferUsageFlags)
     */
    Buffer(cth::not_null<BasicCore const*> core, size_t element_count, VkBufferUsageFlags usage_flags);

    /**
     * @brief constructs and wraps
     * @note calls @ref Buffer(cth::not_null<BasicCore const*>, size_t, VkBufferUsageFlags)
     * @note calls @ref BaseBuffer::wrap()
     */
    Buffer(cth::not_null<BasicCore const*> core, size_t element_count, VkBufferUsageFlags usage_flags, State state);

    /**
     * @brief constructs and creates
     * @note calls @ref Buffer(cth::not_null<BasicCore const*>, size_t, VkBufferUsageFlags)
     * @note calls @ref BaseBuffer::create()
     */
    Buffer(cth::not_null<BasicCore const*> core, size_t element_count, VkBufferUsageFlags usage_flags,
        VkMemoryPropertyFlags memory_property_flags);


    ~Buffer() override = default;

    // ReSharper disable CppHidingFunction

    /**
     *@brief maps part of the buffer memory
    * @param size in elements
    * @param offset in elements
    * @return mapped memory range
    * @note calls @ref BaseBuffer::map(size_t, size_t)
    */
    [[nodiscard]] std::span<T> map(size_t size, size_t offset);

    /**
    * @brief maps whole buffer
    * @return mapped memory range
    * @note calls @ref BaseBuffer::map()
    */
    std::span<T> map();

    /**
    * @brief writes to the mapped range of the whole buffer
    * @note calls @ref BaseBuffer::write()
    */
    void write(std::span<T const> data, size_t mapped_offset = 0) const;

    /**
    * @brief updates non-coherent host visible memory
    * @param size in elements, Constants::WHOLE_SIZE -> whole buffer
    * @param offset in elements
    * @note calls @ref BaseBuffer::flush()
     */
    void flush(size_t size = constants::WHOLE_SIZE, size_t offset = 0) const;

    /**
     * @param size in elements
     * @param offset in elements
     * @note calls @ref BaseBuffer::invalidate()
     */
    void invalidate(size_t size = constants::WHOLE_SIZE, size_t offset = 0) const;

    // ReSharper restore CppHidingFunction

    /**
    * @brief copies buffer data on the gpu
    * @param cmd_buffer
    * @param copy_size in elements (Constants::WHOLE_SIZE => whole buffer)
    * @param src_offset in elements
    * @param dst_offset in elements
    * @note calls @ref BaseBuffer::copy()
    */
    void copy(CmdBuffer const& cmd_buffer, Buffer const& src, size_t copy_size = constants::WHOLE_SIZE, size_t src_offset = 0,
        size_t dst_offset = 0) const;

    /**
     * @brief stages a device local buffer with a temporary host visible buffer
     * @param dst_offset in elements
     * @note calls @ref BaseBuffer::stage()
     */
    void stage(CmdBuffer const& cmd_buffer, Buffer const& staging_buffer, size_t dst_offset = 0) const;

    /**
    * @param size in elements, Constants::WHOLE_SIZE -> whole buffer
    * @param offset in elements
    */
    [[nodiscard]] VkDescriptorBufferInfo descriptorInfo(size_t size, size_t offset) const override;

    /**
     * @brief writes to a mapped memory range
     * @note calls @ref BaseBuffer::write()
     */
    static void write(std::span<T const> data, std::span<T> mapped_memory);

private:
    size_t _elements;

public:
    [[nodiscard]] uint32_t elements() const { return _elements; }

    Buffer(Buffer const& other) = delete;
    Buffer(Buffer&& other) = default;
    Buffer& operator=(Buffer const& other) = delete;
    Buffer& operator=(Buffer&& other) = default;
};

} // namespace cth

#include "CthBuffer.inl"
