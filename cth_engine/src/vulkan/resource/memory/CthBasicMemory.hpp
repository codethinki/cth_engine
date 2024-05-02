#pragma once


#include <vulkan/vulkan_core.h>

#include <span>

#include "../buffer/CthBasicBuffer.hpp"

namespace cth {
class DeletionQueue;
}

namespace cth {
class Device;

using std::span;

/**
 * \brief VkDeviceMemory wrapper without ownership
 */
class BasicMemory {
public:
    /**
     * \param device must not be nullptr
     */
    BasicMemory(Device* device, VkMemoryPropertyFlags vk_properties);
    /**
     * \param device must not be nullptr
     * \note implicitly calls wrap(...);
     */
    BasicMemory(Device* device, VkMemoryPropertyFlags properties, size_t byte_size, VkDeviceMemory vk_memory);
    virtual ~BasicMemory() = default;

    /**
     * \brief wraps existing memory
     */
    virtual void wrap(VkDeviceMemory vk_memory, size_t byte_size);

    /**
     * \brief allocates memory according to the requirements
     * \note does not free previous memory
     */
    virtual void alloc(const VkMemoryRequirements& vk_requirements);

    [[nodiscard]] span<char> map(size_t map_size = VK_WHOLE_SIZE, size_t offset = 0) const;
    [[nodiscard]] VkResult flush(size_t size = VK_WHOLE_SIZE, size_t offset = 0) const;

    [[nodiscard]] VkResult invalidate(size_t size = VK_WHOLE_SIZE, size_t offset = 0) const;
    void unmap() const;

    /**
     * \brief frees the memory and resets the object
     * \param deletion_queue deletion_queue != nullptr => submit to deletion queue
     */
    virtual void free(DeletionQueue* deletion_queue = nullptr);


    /**
     * \brief resets the memory
     * \note does not free memory
     */
    virtual void reset();

    static void free(const Device* device, VkDeviceMemory memory);

protected:
    void init() const;

private:
    Device* _device;
    VkMemoryPropertyFlags _vkProperties;
    size_t _size = 0;
    memory::basic_ptr<VkDeviceMemory_T> _handle = VK_NULL_HANDLE;

public:
    [[nodiscard]] bool allocated() const { return _handle != VK_NULL_HANDLE; }
    [[nodiscard]] VkDeviceMemory get() const { return _handle.get(); }
    [[nodiscard]] size_t size() const { return _size; }
    [[nodiscard]] VkMemoryPropertyFlags properties() const { return _vkProperties; }

    BasicMemory(const BasicMemory& other) = default;
    BasicMemory(BasicMemory&& other) = default;
    BasicMemory& operator=(const BasicMemory& other) = default;
    BasicMemory& operator=(BasicMemory&& other) = default;
#ifdef _DEBUG

    static void debug_check(const BasicMemory* memory);
    static void debug_check_leak(const BasicMemory* memory);
#define DEBUG_CHECK_MEMORY(memory_ptr) BasicMemory::debug_check(memory_ptr)
#define DEBUG_CHECK_MEMORY_LEAK(memory_ptr) BasicMemory::debug_check_leak(memory_ptr)
#else
#define DEBUG_CHECK_MEMORY(memory_ptr) ((void)0)
#define DEBUG_CHECK_MEMORY_LEAK(memory_ptr)  ((void)0)
#endif

};

} // namespace cth