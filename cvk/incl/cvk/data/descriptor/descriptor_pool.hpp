#pragma once
#include "descriptor_pool_config.hpp"

#include "cvk/base/device_table.hpp"
#include "cvk/utility/constants.hpp"
#include "cvk/utility/handle/core_handle.hpp"
#include "cvk/utility/handle/destroy.hpp"
#include "cvk/utility/types.hpp"

#include <volk.h>

#include <cth/io/log.hpp>

#include <cstdint>
#include <tuple>


namespace cvk {
class Core;

/**
 * `VkDescriptorPool` wrapper
 * @details counts its descriptor sets and its resets
 * @details the sets allocate themselves
 * @see `DescriptorSet::create()`
 */
class DescriptorPool {
public:
    using Config = DescriptorPoolConfig;
    struct State;

    /**
     * base constructor
     * @pre `Core::created()`
     */
    DescriptorPool(Core const&, Config);

    /**
     * constructs and wraps
     * @details calls `DescriptorPool()`, `wrap()`
     */
    DescriptorPool(Core const&, Config, State const&);

    /**
     * constructs and creates
     * @details calls `DescriptorPool()`, `create()`
     */
    explicit DescriptorPool(Core const&, Config, create_t);

    /**
     * @note calls `optDestroy()`
     */
    ~DescriptorPool() { optDestroy(); }

    /**
     * wraps the state
     * @pre the pool was created with the config
     * @post `created()`
     * @note calls `optDestroy()`
     */
    void wrap(State const&);

    /**
     * creates the pool
     * @post `created()`
     * @note calls `optDestroy()`
     * @throws cvk::vk_result_exception result of `vkCreateDescriptorPool()`
     */
    void create();

    /**
     * destroys and forgets
     * @pre `created()`
     * @pre `unused()`
     */
    void destroy();

    /**
     * if `created()` -> calls `destroy()`
     */
    void optDestroy() { if(created()) destroy(); }

    /**
     * releases ownership, returns the state and forgets
     * @pre `created()`
     * @pre `unused()`
     */
    State release();

    /**
     * frees every set of the pool
     * @pre `created()`
     * @pre the gpu does not use a set of the pool
     * @post `unused()`, `DescriptorSet::created()` of every set of the pool is `false`
     * @throws cvk::vk_result_exception result of `vkResetDescriptorPool()`
     */
    void reset() const;

    /**
     * counts a set of the pool
     * @details for a released set that the caller keeps, the sets count themselves
     */
    void add() const;

    /**
     * stops to count a set of the pool
     * @details for a counted set that the caller frees, the sets count themselves
     * @pre `!unused()`
     */
    void remove() const;

    [[nodiscard]] constexpr bool created() const { return static_cast<bool>(_handle); }
    [[nodiscard]] constexpr VkDescriptorPool get() const { return _handle.get(); }
    [[nodiscard]] constexpr Core const& core() const { return _handle.core(); }
    [[nodiscard]] constexpr Config const& config() const { return _config; }

    /**
     * @return no set of the pool exists
     */
    [[nodiscard]] constexpr bool unused() const { return _sets == 0; }

    /**
     * @return number of `reset()` calls
     */
    [[nodiscard]] constexpr uint64_t resets() const { return _resets; }

    /**
     * @return `VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT` is set
     */
    [[nodiscard]] constexpr bool freesSets() const {
        return (_config.flags & VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT) != 0;
    }

private:
    void forget();

    core_handle<VkDescriptorPool> _handle;
    Config _config;

    mutable size_t _sets = 0;
    mutable uint64_t _resets = 0;

public:
    DescriptorPool(DescriptorPool const& other) = delete;
    DescriptorPool(DescriptorPool&& other) noexcept = default;
    DescriptorPool& operator=(DescriptorPool const& other) = delete;
    DescriptorPool& operator=(DescriptorPool&& other) noexcept = default;

    static constexpr void debug_check(DescriptorPool const&);
};
}

namespace cvk {
struct DescriptorPool::State {
    vk_not_null<VkDescriptorPool> vkPool;

    /**
     * the raw handles to destroy, in destruction order
     */
    [[nodiscard]] constexpr std::tuple<VkDescriptorPool> decompose() && { return {vkPool.get()}; }
};
}

namespace cvk {
constexpr void DescriptorPool::debug_check(DescriptorPool const& pool) {
    CTH_CRITICAL(!pool.created(), "descriptor pool must be created") {}
}
}
