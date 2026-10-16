#pragma once
#include "memory_config.hpp"

#include "cvk/utility/types.hpp"
#include "cvk/utility/vma_handle.hpp"

#include <volk.h>
#include <vk_mem_alloc.h>

#include <cth/io/log.hpp>

#include <algorithm>
#include <span>


namespace cvk {
class Core;

/**
 * @brief VmaAllocation wrapper, a range of device memory
 * @details VMA places many allocations in one VkDeviceMemory, @ref info() returns the range
 * @attention calls with one memory are not thread safe
 */
class Memory {
public:
    using Config = MemoryConfig;
    struct State;

    /**
     * @brief base constructor
     * @pre @ref Core::created()
     */
    Memory(Core const&, Config const&);

    /**
     * @brief constructs and wraps
     * @details calls:
     * - @ref Memory(Core const&, Config const&)
     * - @ref wrap()
     */
    Memory(Core const&, Config const&, State const&);

    /**
     * @brief constructs and creates
     * @details calls:
     * - @ref Memory(Core const&, Config const&)
     * - @ref create()
     */
    Memory(Core const&, Config const&, VkMemoryRequirements const&);

    /**
     * @note calls @ref optDestroy()
     */
    ~Memory() { optDestroy(); }

    /**
     * @brief wraps the state
     * @pre the allocator of the core created the allocation
     * @post @ref created()
     * @note calls @ref optDestroy()
     */
    void wrap(State const&);

    /**
     * @brief allocates memory for the requirements
     * @pre !@ref Config::autoUsage(), VMA_MEMORY_USAGE_AUTO* needs a buffer or an image
     * @post @ref created()
     * @note calls @ref optDestroy()
     * @throws cvk::vk_result_exception result of vmaAllocateMemory()
     */
    void create(VkMemoryRequirements const&);

    /**
     * @brief frees the memory and forgets
     * @pre @ref created()
     * @pre the gpu does not use the memory
     */
    void destroy();

    /**
     * @brief if @ref created() -> calls @ref destroy()
     */
    void optDestroy() { if(created()) destroy(); }

    /**
     * @brief releases ownership, returns the state and forgets
     * @pre @ref created()
     */
    State release();

    /**
     * @brief maps the memory
     * @param size (VK_WHOLE_SIZE => to the end)
     * @param offset from the start of the memory
     * @return the mapped range
     * @pre @ref created()
     * @pre @ref properties() has VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
     * @pre offset + size <= @ref size()
     * @note VMA counts the maps, each call needs one @ref unmap()
     * @throws cvk::vk_result_exception result of vmaMapMemory()
     */
    [[nodiscard]] std::span<char> map(size_t size = VK_WHOLE_SIZE, size_t offset = 0) const;

    /**
     * @brief unmaps the memory once
     * @pre @ref map() was called more often than @ref unmap()
     */
    void unmap() const;

    /**
     * @brief makes host writes visible to the device
     * @param size (VK_WHOLE_SIZE => to the end)
     * @param offset from the start of the memory
     * @details host coherent memory -> does nothing
     * @pre @ref created()
     * @throws cvk::vk_result_exception result of vmaFlushAllocation()
     */
    void flush(size_t size = VK_WHOLE_SIZE, size_t offset = 0) const;

    /**
     * @brief makes device writes visible to the host
     * @param size (VK_WHOLE_SIZE => to the end)
     * @param offset from the start of the memory
     * @details host coherent memory -> does nothing
     * @pre @ref created()
     * @throws cvk::vk_result_exception result of vmaInvalidateAllocation()
     */
    void invalidate(size_t size = VK_WHOLE_SIZE, size_t offset = 0) const;

    /**
     * @brief copies the data to the memory
     * @details maps, copies, flushes and unmaps
     * @param offset from the start of the memory
     * @pre @ref created()
     * @pre @ref properties() has VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
     * @pre offset + data.size() <= @ref size()
     * @throws cvk::vk_result_exception result of vmaMapMemory()
     * @throws cvk::vk_result_exception result of vmaFlushAllocation()
     */
    void write(std::span<char const> data, size_t offset = 0) const;

    /**
     * @brief copies the data to the mapped range
     * @pre data.size() <= mapped.size()
     */
    static constexpr void write(std::span<char const> data, std::span<char> mapped);

    static void destroy(VmaAllocator, VmaAllocation);

    [[nodiscard]] constexpr bool created() const { return static_cast<bool>(_handle); }
    [[nodiscard]] constexpr VmaAllocation get() const { return _handle.get(); }
    [[nodiscard]] constexpr Core const& core() const { return _handle.core(); }
    [[nodiscard]] constexpr Config const& config() const { return _config; }

    /**
     * @return the VkDeviceMemory, the offset and the size of the range
     * @pre @ref created()
     */
    [[nodiscard]] VmaAllocationInfo info() const;

    /**
     * @return the property flags of the memory type that VMA picked
     * @pre @ref created()
     */
    [[nodiscard]] VkMemoryPropertyFlags properties() const;

    /**
     * @pre @ref created()
     */
    [[nodiscard]] size_t size() const { return info().size; }

    /**
     * @return the whole memory if mapped, not mapped -> empty
     * @details @ref map() or VMA_ALLOCATION_CREATE_MAPPED_BIT maps it
     * @pre @ref created()
     */
    [[nodiscard]] std::span<char> mapped() const;

private:
    void forget();

    [[nodiscard]] VmaAllocator allocator() const;

    vma_handle<VmaAllocation, Memory> _handle;
    Config _config;

public:
    Memory(Memory const& other) = delete;
    Memory(Memory&& other) noexcept = default;
    Memory& operator=(Memory const& other) = delete;
    Memory& operator=(Memory&& other) noexcept = default;

    static constexpr void debug_check(Memory const&);
};
}

namespace cvk {
struct Memory::State {
    vk_not_null<VmaAllocation> vmaAllocation;
};
}

namespace cvk {
constexpr void Memory::write(std::span<char const> data, std::span<char> mapped) {
    CTH_CRITICAL(data.size() > mapped.size(), "data must fit into the mapped range") {}
    std::ranges::copy(data, mapped.begin());
}
}

namespace cvk {
constexpr void Memory::debug_check(Memory const& memory) {
    CTH_CRITICAL(!memory.created(), "memory must be created") {}
}
}
