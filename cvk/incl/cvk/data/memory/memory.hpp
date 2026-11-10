#pragma once
#include "mapping.hpp"
#include "memory_config.hpp"

#include "cvk/utility/handle/core_handle.hpp"
#include "cvk/utility/handle/destroy.hpp"
#include "cvk/utility/range.hpp"
#include "cvk/utility/types.hpp"

#include <volk.h>
#include <vk_mem_alloc.h>

#include <cth/io/log.hpp>

#include <concepts>
#include <cstddef>
#include <memory>
#include <span>
#include <tuple>


namespace cvk {
class Core;

/**
 * `VmaAllocation` wrapper
 * @details a range of a `VkDeviceMemory`
 * @attention not thread safe
 * @see `info()`
 */
class Memory {
public:
    using Config = MemoryConfig;
    struct State;

    /**
     * base constructor
     */
    Memory(Core const&, Config const&);

    /**
     * constructs and wraps
     * @details calls @ref Memory(Core const&, Config const&), `wrap()`
     */
    Memory(Core const&, Config const&, State const&);

    /**
     * constructs and creates
     * @details calls @ref Memory(Core const&, Config const&), `create()`
     */
    Memory(Core const&, Config const&, VkMemoryRequirements const&);

    /**
     * @note calls `optDestroy()`
     */
    ~Memory() { optDestroy(); }

    /**
     * wraps the state
     * @pre `Core::created()`
     * @pre the allocator of the core created the allocation
     * @post `created()`
     * @note calls `optDestroy()`
     */
    void wrap(State const&);

    /**
     * allocates memory for the requirements
     * @pre `Core::created()`
     * @pre `!Config::autoUsage()`
     * @post `created()`
     * @note calls `optDestroy()`
     * @throws cvk::vk_result_exception result of `vmaAllocateMemory()`
     */
    void create(VkMemoryRequirements const&);

    /**
     * frees the memory and forgets
     * @pre `created()`
     * @pre the gpu does not use the memory
     */
    void destroy();

    /**
     * if `created()` -> calls `destroy()`
     */
    void optDestroy() { if(created()) destroy(); }

    /**
     * releases ownership, returns the state and forgets
     * @pre `created()`
     */
    State release();

    /**
     * maps the range of the memory
     * @details a map while a `Mapping` lives returns the same memory
     * @param range in bytes
     * @return the mapped range, its destructor unmaps
     * @pre `created()`
     * @pre `properties()` has `VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT`
     * @pre the range lies in the memory
     * @throws cvk::vk_result_exception result of `vmaMapMemory()`
     */
    [[nodiscard]] Mapping<std::byte> map(Range range = {}) const;

    /**
     * makes host writes visible to the device
     * @param range in bytes
     * @details host coherent memory -> does nothing
     * @pre `created()`
     * @pre the memory is mapped: a `Mapping` lives or `VMA_ALLOCATION_CREATE_MAPPED_BIT`
     * @throws cvk::vk_result_exception result of `vmaFlushAllocation()`
     */
    void flush(Range range = {}) const;

    /**
     * makes device writes visible to the host
     * @param range in bytes
     * @details host coherent memory -> does nothing
     * @pre `created()`
     * @pre the memory is mapped: a `Mapping` lives or `VMA_ALLOCATION_CREATE_MAPPED_BIT`
     * @throws cvk::vk_result_exception result of `vmaInvalidateAllocation()`
     */
    void invalidate(Range range = {}) const;

    /**
     * copies the data to the memory
     * @param offset in bytes
     * @pre `created()`
     * @pre `properties()` has `VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT`
     * @pre the data fits into the memory after the offset
     * @throws cvk::vk_result_exception result of `vmaMapMemory()`
     * @throws cvk::vk_result_exception result of `vmaFlushAllocation()`
     */
    void write(std::span<std::byte const> data, size_t offset = 0) const;

    [[nodiscard]] constexpr bool created() const { return static_cast<bool>(_handle); }
    [[nodiscard]] constexpr VmaAllocation get() const { return _handle.get(); }
    [[nodiscard]] constexpr Core const& core() const { return _handle.core(); }
    [[nodiscard]] constexpr Config const& config() const { return _config; }

    /**
     * @return the `VkDeviceMemory`, the offset and the size of the range
     * @pre `created()`
     */
    [[nodiscard]] VmaAllocationInfo info() const;

    /**
     * @return the property flags of the memory type that VMA picked
     * @pre `created()`
     */
    [[nodiscard]] VkMemoryPropertyFlags properties() const;

    /**
     * @pre `created()`
     */
    [[nodiscard]] size_t size() const { return info().size; }

    /**
     * @return the whole memory if mapped, not mapped -> empty
     * @details a living `Mapping` or `VMA_ALLOCATION_CREATE_MAPPED_BIT` maps it
     * @pre `created()`
     */
    [[nodiscard]] std::span<std::byte> mapped() const;

private:
    template<class T>
    friend class Mapping;

    /**
     * unmaps the memory once, the destructor of a `Mapping` calls it
     */
    void unmap() const;

    void forget();

    [[nodiscard]] VmaAllocator allocator() const;

    core_handle<VmaAllocation> _handle;
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

    /**
     * the raw handles to destroy, in destruction order
     */
    [[nodiscard]] constexpr std::tuple<VmaAllocation> decompose() && { return {vmaAllocation.get()}; }
};
}

namespace cvk {
template<class T>
Mapping<T>::~Mapping() {
    if(_memoryPtr != nullptr) _memoryPtr->unmap();
}

template<class T>
template<class U> requires std::same_as<T, std::byte>
Mapping<U> Mapping<T>::as() && {
    CTH_CRITICAL(_range.size() % sizeof(U) != 0, "the size must be a multiple of the element size") {}

    size_t const count = _range.size() / sizeof(U);
    auto const& memory = *_memoryPtr.release();
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
