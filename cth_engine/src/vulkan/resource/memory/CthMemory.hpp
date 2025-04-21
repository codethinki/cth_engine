#pragma once
#include "src/vulkan/base/CthDeviceTable.hpp"
#include "src/vulkan/utility/cth_constants.hpp"
#include "src/vulkan/utility/cth_vk_types.hpp"

#include <cth/pointers.hpp>

#include <gsl/pointers>

#include <volk.h>



namespace cth::vk {
class Core;
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
    Memory(Core const& core, VkMemoryPropertyFlags vk_properties);

    /**
     * @param core must not be nullptr
     * @note calls @ref alloc();
     */
    Memory(Core const& core, VkMemoryPropertyFlags properties, VkMemoryRequirements const& vk_requirements);
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


    static void destroy(DeviceTable table, VkDeviceMemory memory);

    /**
     * @brief releases the handle and resets the object
     * @note the handle must be destroyed by the caller
     */
    State release();

private:
    void reset();

    cth::not_null<Core const*> _core;
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

    static void debug_check(Memory const* memory);
    static void debug_check_handle(VkDeviceMemory vk_memory);
};

} // namespace cth

//State

namespace cth::vk {
struct Memory::State {
    vk::not_null<VkDeviceMemory> vkMemory; // NOLINT(cppcoreguidelines-owning-memory)
    size_t size; //in bytes
};

}

//debug check

namespace cth::vk {
inline void Memory::debug_check(Memory const* memory) {
    CTH_CRITICAL(memory == nullptr, "memory must not be nullptr") {}
    CTH_CRITICAL(!memory->created(), "memory must be created") {}

    debug_check_handle(memory->get());
}
inline void Memory::debug_check_handle(VkDeviceMemory vk_memory) {
    CTH_CRITICAL(vk_memory == VK_NULL_HANDLE, "memory handle should not be invalid (VK_NULL_HANDLE)"){}
}
}
