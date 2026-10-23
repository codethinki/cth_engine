#pragma once
#include "mapping.hpp"
#include "memory_config.hpp"

#include "cvk/utility/handle/vma_handle.hpp"
#include "cvk/utility/range.hpp"
#include "cvk/utility/types.hpp"

#include <volk.h>
#include <vk_mem_alloc.h>

#include <cth/io/log.hpp>

#include <concepts>
#include <cstddef>
#include <memory>
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
    using vk_handle_t = VmaAllocation;
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
     * @brief maps the range of the memory
     * @param range in bytes
     * @return the mapped range, its destructor unmaps
     * @pre @ref created()
     * @pre @ref properties() has VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
     * @pre the range lies in the memory
     * @note VMA counts the maps, a map while a mapping lives returns the same memory
     * @throws cvk::vk_result_exception result of vmaMapMemory()
     */
    [[nodiscard]] Mapping<std::byte> map(Range range = {}) const;

    /**
     * @brief makes host writes visible to the device
     * @param range in bytes
     * @details host coherent memory -> does nothing
     * @pre @ref created()
     * @pre the memory is mapped: a @ref Mapping lives or VMA_ALLOCATION_CREATE_MAPPED_BIT
     * @throws cvk::vk_result_exception result of vmaFlushAllocation()
     */
    void flush(Range range = {}) const;

    /**
     * @brief makes device writes visible to the host
     * @param range in bytes
     * @details host coherent memory -> does nothing
     * @pre @ref created()
     * @pre the memory is mapped: a @ref Mapping lives or VMA_ALLOCATION_CREATE_MAPPED_BIT
     * @throws cvk::vk_result_exception result of vmaInvalidateAllocation()
     */
    void invalidate(Range range = {}) const;

    /**
     * @brief copies the data to the memory
     * @details maps, copies, flushes and unmaps
     * @param offset in bytes
     * @pre @ref created()
     * @pre @ref properties() has VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
     * @pre the data fits into the memory after the offset
     * @throws cvk::vk_result_exception result of vmaMapMemory()
     * @throws cvk::vk_result_exception result of vmaFlushAllocation()
     */
    void write(std::span<std::byte const> data, size_t offset = 0) const;

    static void destroy(VmaAllocator, VmaAllocation);

    [[nodiscard]] constexpr bool created() const { return static_cast<bool>(_handle); }
    [[nodiscard]] constexpr vk_handle_t get() const { return _handle.get(); }
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
     * @details a living @ref Mapping or VMA_ALLOCATION_CREATE_MAPPED_BIT maps it
     * @pre @ref created()
     */
    [[nodiscard]] std::span<std::byte> mapped() const;

private:
    template<class T>
    friend class Mapping;

    /**
     * @brief unmaps the memory once, the destructor of a @ref Mapping calls it
     */
    void unmap() const;

    void forget();

    [[nodiscard]] VmaAllocator allocator() const;

    vma_handle<Memory> _handle;
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
template<class T>
Mapping<T>::~Mapping() {
    if(_memory != nullptr) _memory->unmap();
}

template<class T>
template<class U> requires std::same_as<T, std::byte>
Mapping<U> Mapping<T>::as() && {
    CTH_CRITICAL(_range.size() % sizeof(U) != 0, "the size must be a multiple of the element size") {}

    size_t const count = _range.size() / sizeof(U);
    auto const& memory = *_memory.release();
#ifdef __cpp_lib_start_lifetime_as
    return Mapping<U>{memory, {std::start_lifetime_as_array<U>(_range.data(), count), count}};
#else
    //TODO remove when the MSVC STL has start_lifetime_as on clang (microsoft/STL GH-6169)
    return Mapping<U>{memory, {reinterpret_cast<U*>(_range.data()), count}};
#endif
}
}

namespace cvk {
constexpr void Memory::debug_check(Memory const& memory) {
    CTH_CRITICAL(!memory.created(), "memory must be created") {}
}
}
