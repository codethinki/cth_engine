#pragma once
#include "cvk/utility/constants.hpp"
#include "cvk/utility/handle/unique_handle.hpp"
#include "cvk/utility/types.hpp"

#include <volk.h>
#include <vk_mem_alloc.h>

#include <cth/io/log.hpp>


namespace cvk {
class Device;

/**
 * @brief VmaAllocator wrapper, allocates the memory of one device
 * @details VMA places many allocations in one VkDeviceMemory,
 *  one allocator per device is enough,
 *  calls with the allocator are thread safe, VMA synchronizes them
 */
class Allocator {
public:
    using vk_handle_t = VmaAllocator;
    struct State;

    /**
     * @brief base constructor
     * @pre @ref Device::created()
     */
    explicit Allocator(Device const&);

    /**
     * @brief constructs and wraps
     * @details calls:
     * - @ref Allocator(Device const&)
     * - @ref wrap()
     */
    Allocator(Device const&, State const&);

    /**
     * @brief constructs and creates
     * @details calls:
     * - @ref Allocator(Device const&)
     * - @ref create()
     */
    explicit Allocator(Device const&, create_t);

    /**
     * @note calls @ref optDestroy()
     */
    ~Allocator() { optDestroy(); }

    /**
     * @brief wraps the state
     * @pre the allocator was created with the device
     * @post @ref created()
     * @note calls @ref optDestroy()
     */
    void wrap(State const&);

    /**
     * @brief creates the allocator
     * @details the flags follow the enabled features and extensions of the device: @ref flags()
     * @post @ref created()
     * @note calls @ref optDestroy()
     * @throws cvk::vk_result_exception result of vmaImportVulkanFunctionsFromVolk()
     * @throws cvk::vk_result_exception result of vmaCreateAllocator()
     */
    void create();

    /**
     * @brief destroys and forgets
     * @pre @ref created()
     * @pre every memory of the allocator is destroyed
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

    static void destroy(VmaAllocator);

    /**
     * @return VMA flags for the enabled features and extensions of the device
     * @details bufferDeviceAddress, maintenance4, VK_EXT_memory_budget
     */
    [[nodiscard]] static VmaAllocatorCreateFlags flags(Device const&);

    [[nodiscard]] constexpr bool created() const { return static_cast<bool>(_handle); }
    [[nodiscard]] constexpr vk_handle_t get() const { return _handle.get(); }
    [[nodiscard]] constexpr Device const& device() const { return *_device; }

private:
    void forget();

    not_null_ptr<Device const> _device;
    unique_handle<vk_handle_t, handle_deleter<Allocator>> _handle;

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
