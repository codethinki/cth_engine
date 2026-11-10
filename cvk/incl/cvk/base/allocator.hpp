#pragma once
#include "cvk/utility/constants.hpp"
#include "cvk/utility/handle/destroy.hpp"
#include "cvk/utility/handle/unique_handle.hpp"
#include "cvk/utility/types.hpp"

#include <volk.h>
#include <vk_mem_alloc.h>

#include <cth/io/log.hpp>
#include <cth/ptr/ref_wrapper.hpp>


namespace cvk {
class Device;

/**
 * `VmaAllocator` wrapper
 * @details allocates the memory of one device
 * @details thread safe
 */
class Allocator {
public:
    struct State;

    /**
     * base constructor
     */
    explicit Allocator(Device const&);

    /**
     * constructs and wraps
     * @details calls @ref Allocator(Device const&), `wrap()`
     */
    Allocator(Device const&, State const&);

    /**
     * constructs and creates
     * @details calls @ref Allocator(Device const&), `create()`
     */
    explicit Allocator(Device const&, create_t);

    /**
     * @note calls `optDestroy()`
     */
    ~Allocator() { optDestroy(); }

    /**
     * wraps the state
     * @pre `Device::created()`
     * @pre the allocator was created with the device
     * @post `created()`
     * @note calls `optDestroy()`
     */
    void wrap(State const&);

    /**
     * creates the allocator
     * @details the flags follow the enabled features and extensions of the device
     * @pre `Device::created()`
     * @post `created()`
     * @note calls `optDestroy()`
     * @throws cvk::vk_result_exception result of `vmaImportVulkanFunctionsFromVolk()`
     * @throws cvk::vk_result_exception result of `vmaCreateAllocator()`
     * @see `flags()`
     */
    void create();

    /**
     * destroys and forgets
     * @pre `created()`
     * @pre every memory of the allocator is destroyed
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
     * @return VMA flags for `bufferDeviceAddress`, `maintenance4` and `VK_EXT_memory_budget` of the device
     */
    [[nodiscard]] static VmaAllocatorCreateFlags flags(Device const&);

    [[nodiscard]] constexpr bool created() const { return static_cast<bool>(_handle); }
    [[nodiscard]] constexpr VmaAllocator get() const { return _handle.get(); }
    [[nodiscard]] constexpr Device const& device() const { return *_device; }

private:
    void forget();

    cth::ref_wrapper<Device const> _device;
    unique_handle<VmaAllocator, handle_deleter> _handle;

public:
    Allocator(Allocator const& other) = delete;
    Allocator(Allocator&& other) noexcept = default;
    Allocator& operator=(Allocator const& other) = delete;
    Allocator& operator=(Allocator&& other) noexcept = default;

    static constexpr void debug_check(Allocator const&);
};
}

namespace cvk {
struct Allocator::State {
    vk_not_null<VmaAllocator> vmaAllocator;
};
}

namespace cvk {
constexpr void Allocator::debug_check(Allocator const& allocator) {
    CTH_CRITICAL(!allocator.created(), "allocator must be created") {}
}
}
