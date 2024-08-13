#pragma once
#include "vulkan/utility/cth_constants.hpp"

#include<cth/cth_pointer.hpp>

#include <vulkan/vulkan.h>



namespace cth::vk {
class BasicCore;
class Device;
class DestructionQueue;


/**
 * @brief VkDeviceMemory wrapper without ownership
 */
class BasicMemory {
public:
    /**
     * @param core must not be nullptr
     */
    BasicMemory(BasicCore const* core, VkMemoryPropertyFlags vk_properties);
    /**
     * @param core must not be nullptr
     * @note implicitly calls wrap();
     */
    BasicMemory(BasicCore const* core, VkMemoryPropertyFlags properties, size_t byte_size, VkDeviceMemory vk_memory);
    virtual ~BasicMemory() = default;

    /**
     * @brief wraps existing memory
     */
    virtual void wrap(VkDeviceMemory vk_memory, size_t byte_size);

    /**
     * @brief allocates memory according to the requirements
     * @note does not free previous memory
     */
    virtual void alloc(VkMemoryRequirements const& vk_requirements);

    [[nodiscard]] std::span<char> map(size_t map_size = VK_WHOLE_SIZE, size_t offset = 0) const;
    [[nodiscard]] VkResult flush(size_t size = VK_WHOLE_SIZE, size_t offset = 0) const;

    [[nodiscard]] VkResult invalidate(size_t size = VK_WHOLE_SIZE, size_t offset = 0) const;
    void unmap() const;

    /**
     * @brief frees the memory and resets the object
     * @param destruction_queue destruction_queue != nullptr => submit to deletion queue
     */
    virtual void free(DestructionQueue* destruction_queue = nullptr);


    /**
     * @brief resets the memory
     * @note does not free memory
     */
    virtual void reset();

    static void free(VkDevice vk_device, VkDeviceMemory memory);

protected:
    void init() const;

private:
    BasicCore const* _core;
    VkMemoryPropertyFlags _vkProperties;
    size_t _size = 0;
    move_ptr<VkDeviceMemory_T> _handle = VK_NULL_HANDLE;

public:
    [[nodiscard]] bool allocated() const { return _handle != VK_NULL_HANDLE; }
    [[nodiscard]] VkDeviceMemory get() const { return _handle.get(); }
    [[nodiscard]] size_t size() const { return _size; }
    [[nodiscard]] VkMemoryPropertyFlags properties() const { return _vkProperties; }

    BasicMemory(BasicMemory const& other) = default;
    BasicMemory(BasicMemory&& other) = default;
    BasicMemory& operator=(BasicMemory const& other) = default;
    BasicMemory& operator=(BasicMemory&& other) = default;
#ifdef CONSTANT_DEBUG_MODE

    static void debug_check(BasicMemory const* memory);
    static void debug_check_leak(BasicMemory const* memory);
#define DEBUG_CHECK_MEMORY(memory_ptr) BasicMemory::debug_check(memory_ptr)
#define DEBUG_CHECK_MEMORY_LEAK(memory_ptr) BasicMemory::debug_check_leak(memory_ptr)
#else
#define DEBUG_CHECK_MEMORY(memory_ptr) ((void)0)
#define DEBUG_CHECK_MEMORY_LEAK(memory_ptr)  ((void)0)
#endif

};

} // namespace cth