#pragma once
#include "vulkan/utility/cth_constants.hpp"

#include<cth/pointers.hpp>

#include <gsl/pointers>

#include <vulkan/vulkan.h>



namespace cth::vk {
class BasicCore;
class Device;
class DestructionQueue;


/**
 * @brief VkDeviceMemory wrapper
 */
class Memory {
public:
    struct State;

    /**
     * @param core must be valid
     */
    Memory(cth::not_null<BasicCore const*> core, VkMemoryPropertyFlags vk_properties);

    /**
     * @param core must not be nullptr
     * @note calls @ref alloc();
     */
    Memory(cth::not_null<BasicCore const*> core, VkMemoryPropertyFlags properties, VkMemoryRequirements const& vk_requirements);
    ~Memory();

    /**
     * @brief wraps existing memory
     * @note calls @ref destroy() if memory is already allocated
     */
    void wrap(State const& state);

    /**
     * @brief allocates memory according to the requirements
     * @note calls @ref destroy() if memory is already allocated
     */
    void create(VkMemoryRequirements const& vk_requirements);

    [[nodiscard]] std::span<char> map(size_t map_size = VK_WHOLE_SIZE, size_t offset = 0) const;
    void flush(size_t size = VK_WHOLE_SIZE, size_t offset = 0) const;

    void invalidate(size_t size = VK_WHOLE_SIZE, size_t offset = 0) const;
    void unmap() const;



    /**
     * @brief frees the memory and resets the object
     */
    void destroy();


    static void destroy(VkDevice vk_device, VkDeviceMemory memory);

    /**
     * @brief releases the handle and resets the object
     * @note the handle must be destroyed by the caller
     */
    State release();

private:
    void reset();

    cth::not_null<BasicCore const*> _core;
    VkMemoryPropertyFlags _vkProperties;
    size_t _size = 0;
    move_ptr<VkDeviceMemory_T> _handle = VK_NULL_HANDLE;

public:
    [[nodiscard]] bool created() const { return _handle != VK_NULL_HANDLE; }
    [[nodiscard]] VkDeviceMemory get() const { return _handle.get(); }
    [[nodiscard]] size_t size() const { return _size; }
    [[nodiscard]] VkMemoryPropertyFlags properties() const { return _vkProperties; }

    Memory(Memory const& other) = default;
    Memory(Memory&& other) = default;
    Memory& operator=(Memory const& other) = default;
    Memory& operator=(Memory&& other) = default;
#ifdef CONSTANT_DEBUG_MODE

    static void debug_check(Memory const* memory);
    static void debug_check_handle(VkDeviceMemory vk_memory);
#define DEBUG_CHECK_MEMORY(memory_ptr) Memory::debug_check(memory_ptr)
#define DEBUG_CHECK_MEMORY_HANDLE(vk_memory) Memory::debug_check_handle(vk_memory)
#else
#define DEBUG_CHECK_MEMORY(memory_ptr) ((void)0)
#define DEBUG_CHECK_MEMORY_HANDLE(vk_memory) ((void)0)
#endif

};

} // namespace cth

//State

namespace cth::vk {
struct Memory::State {
    gsl::owner<VkDeviceMemory> vkMemory = VK_NULL_HANDLE; // NOLINT(cppcoreguidelines-owning-memory)
    size_t size = 0; //in bytes
};

}
