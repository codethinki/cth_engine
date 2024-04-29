#pragma once
#include <cth/cth_log.hpp>

#include <vulkan/vulkan.h>

#include <span>

#include "vulkan/utility/CthConstants.hpp"



namespace cth {
class BasicMemory;
class CmdBuffer;
class Device;
class DeletionQueue;
class Memory;

using std::span;
using std::unique_ptr;

class BasicBuffer {
public:
    struct State;

    /**
     * \param buffer_size in bytes
     */
    BasicBuffer(Device* device, size_t buffer_size, VkBufferUsageFlags usage_flags);
    BasicBuffer(Device* device, size_t buffer_size, VkBufferUsageFlags usage_flags, VkBuffer vk_buffer, const State& state);

    virtual ~BasicBuffer() = default;


    virtual void wrap(VkBuffer vk_buffer, const State& state);

    /**
    * \brief creates the image
    * \note buffer must not be a valid handle
    */
    virtual void create();

    /**
    * \brief allocates buffer memory
    * \param new_memory must not be allocated or nullptr
    * \note implicitly calls setMemory(new_memory)
    */
    void alloc(BasicMemory* new_memory);
    /**
    * \brief allocates buffer memory
    * \note memory must not be allocated
    */
    void alloc() const;

    /**
    * \brief binds buffer to new_memory and replaces the old one
    * \param new_memory must be allocated
    * \note implicitly calls setMemory(new_memory)
    */
    void bind(BasicMemory* new_memory);
    /**
    * binds buffer to memory
    * \note memory must be allocated
    */
    void bind() const;


    /**
    * \brief destroys the buffer
    * \param deletion_queue != nullptr => submit to deletion queue
    * \note memory will not be reset
    */
    virtual void destroy(DeletionQueue* deletion_queue);

    /**
    * \param new_memory must not be allocated, nullptr or current memory
    * \note does not free current memory
    */
    virtual void setMemory(BasicMemory* new_memory);

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
    void stage(const CmdBuffer& cmd_buffer, const BasicBuffer& staging_buffer, size_t dst_offset = 0) const;

    /**
     * \brief writes to the mapped range of the whole buffer
     * \note whole buffer must be mapped first
     * \note not virtual
     */
    void write(span<const char> data, size_t buffer_offset = 0) const;


    /**
    * \brief copies buffer data on the gpu
    * \param cmd_buffer
    * \param copy_size in bytes (Constants::WHOLE_SIZE => whole buffer)
    * \param src_offset in bytes
    * \param dst_offset in bytes
     */
    void copy(const CmdBuffer& cmd_buffer, const BasicBuffer& src, size_t copy_size = Constants::WHOLE_SIZE, size_t src_offset = 0,
        size_t dst_offset = 0) const;


    /**
     * \brief updates non-coherent host visible memory
     * \param size in bytes, Constants::WHOLE_SIZE -> whole buffer
     * \param offset in bytes
     */
    [[nodiscard]] VkResult flush(size_t size = Constants::WHOLE_SIZE, size_t offset = 0) const;

    /**
    * \brief
    * \param size in elements
    * \param offset in elements
    * \return result of vkInvalidateMappedMemoryRanges()
    */
    [[nodiscard]] VkResult invalidate(size_t size = Constants::WHOLE_SIZE, size_t offset = 0) const;


    /**
    * \param size in bytes, Constants::WHOLE_SIZE -> whole buffer
    * \param offset in bytes
    */
    [[nodiscard]] virtual VkDescriptorBufferInfo descriptorInfo(size_t size, size_t offset) const;

    /**
    * \brief writes to a mapped memory range
    */
    static void write(span<const char> data, span<char> mapped_memory);

    static void destroy(const Device* device, VkBuffer vk_buffer);

    /**
    * \brief aligns the buffer size to 16 bc I don't understand buffer alignment
    * \param actual_size in bytes
    */
    static size_t calcAlignedSize(size_t actual_size);

    struct State {
        BasicMemory* memory;
        bool bound = memory != nullptr;
        span<char> mapped{}; //must not specify an offset into the buffer

        static State Default() { return State{}; }

    private:
        void reset();
        friend BasicBuffer;
    };

protected:
    void reset();

    Device* device;
    size_t size_;
    VkBufferUsageFlags usage_;


    State state_ = State::Default();
private:
    void init() const;

    VkBuffer vkBuffer = VK_NULL_HANDLE;
public:
    [[nodiscard]] auto get() const { return vkBuffer; }
    [[nodiscard]] auto* memory() const { return state_.memory; }
    [[nodiscard]] auto bound() const { return state_.bound; }
    [[nodiscard]] auto mapped() const { return state_.mapped; }
    [[nodiscard]] auto usageFlags() const { return usage_; }
    [[nodiscard]] auto size() const { return size_; }

    [[nodiscard]] auto state() const { return state_; }

    BasicBuffer(const BasicBuffer& other) = delete;
    BasicBuffer& operator=(const BasicBuffer& other) = delete;
    BasicBuffer(BasicBuffer&& other) = delete;
    BasicBuffer& operator=(BasicBuffer&& other) = delete;

#ifdef _DEBUG
    static void debug_check(const BasicBuffer* buffer);
    static void debug_check_not_bound(const BasicBuffer* buffer);
#else
    static void debug_check(const BasicBuffer* buffer) {}
    static void debug_check_not_bound(const BasicBuffer* buffer) {}
#endif
};
} // namespace cth
