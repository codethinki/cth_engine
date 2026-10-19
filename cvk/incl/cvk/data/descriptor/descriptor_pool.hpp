#pragma once
#include "descriptor_pool_config.hpp"

#include "cvk/base/device_table.hpp"
#include "cvk/utility/constants.hpp"
#include "cvk/utility/handle/core_handle.hpp"
#include "cvk/utility/types.hpp"

#include <volk.h>

#include <cth/io/log.hpp>

#include <cstdint>


namespace cvk {
class Core;

/**
 * @brief VkDescriptorPool wrapper, counts its descriptor sets and its resets
 * @details the sets allocate themselves, see @ref DescriptorSet::create()
 */
class DescriptorPool {
public:
    using vk_handle_t = VkDescriptorPool;
    using Config = DescriptorPoolConfig;
    struct State;

    /**
     * @brief base constructor
     * @pre @ref Core::created()
     */
    DescriptorPool(Core const&, Config);

    /**
     * @brief constructs and wraps
     * @details calls:
     * - @ref DescriptorPool(Core const&, Config)
     * - @ref wrap()
     */
    DescriptorPool(Core const&, Config, State const&);

    /**
     * @brief constructs and creates
     * @details calls:
     * - @ref DescriptorPool(Core const&, Config)
     * - @ref create()
     */
    explicit DescriptorPool(Core const&, Config, create_t);

    /**
     * @note calls @ref optDestroy()
     */
    ~DescriptorPool() { optDestroy(); }

    /**
     * @brief wraps the state
     * @pre the pool was created with the config
     * @post @ref created()
     * @note calls @ref optDestroy()
     */
    void wrap(State const&);

    /**
     * @brief creates the pool
     * @post @ref created()
     * @note calls @ref optDestroy()
     * @throws cvk::vk_result_exception result of vkCreateDescriptorPool()
     */
    void create();

    /**
     * @brief destroys and forgets
     * @pre @ref created()
     * @pre @ref unused()
     */
    void destroy();

    /**
     * @brief if @ref created() -> calls @ref destroy()
     */
    void optDestroy() { if(created()) destroy(); }

    /**
     * @brief releases ownership, returns the state and forgets
     * @pre @ref created()
     * @pre @ref unused()
     */
    State release();

    /**
     * @brief frees every set of the pool
     * @pre @ref created()
     * @pre the gpu does not use a set of the pool
     * @post @ref unused(), @ref DescriptorSet::created() of every set of the pool is false
     * @throws cvk::vk_result_exception result of vkResetDescriptorPool()
     */
    void reset() const;

    /**
     * @brief counts a set of the pool
     * @details @ref DescriptorSet::create() and @ref DescriptorSet::wrap() call it,
     *  code that keeps a released set of the pool may call it too
     */
    void add() const;

    /**
     * @brief stops to count a set of the pool
     * @details @ref DescriptorSet::destroy() and @ref DescriptorSet::release() call it,
     *  code that frees a counted set itself calls it too
     * @pre !@ref unused()
     */
    void remove() const;

    static void destroy(DeviceTable table, VkDescriptorPool);

    [[nodiscard]] constexpr bool created() const { return static_cast<bool>(_handle); }
    [[nodiscard]] constexpr vk_handle_t get() const { return _handle.get(); }
    [[nodiscard]] constexpr Core const& core() const { return _handle.core(); }
    [[nodiscard]] constexpr Config const& config() const { return _config; }

    /**
     * @return no set of the pool exists
     */
    [[nodiscard]] constexpr bool unused() const { return _sets == 0; }

    /**
     * @return number of @ref reset() calls
     */
    [[nodiscard]] constexpr uint64_t resets() const { return _resets; }

    /**
     * @return VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT is set
     */
    [[nodiscard]] constexpr bool freesSets() const {
        return (_config.flags & VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT) != 0;
    }

private:
    void forget();

    core_handle<DescriptorPool> _handle;
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
};
}

namespace cvk {
constexpr void DescriptorPool::debug_check(DescriptorPool const& pool) {
    CTH_CRITICAL(!pool.created(), "descriptor pool must be created") {}
}
}
