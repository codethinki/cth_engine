#pragma once
#include "cmd_pool_config.hpp"

#include "cvk/base/device_table.hpp"
#include "cvk/utility/constants.hpp"
#include "cvk/utility/handle/core_handle.hpp"
#include "cvk/utility/handle/destroy.hpp"
#include "cvk/utility/types.hpp"

#include <volk.h>

#include <cth/io/log.hpp>

#include <tuple>


namespace cvk {
class Core;

/**
 * `VkCommandPool` wrapper
 * @details counts its command buffers and its resets
 * @attention not thread safe, also through its command buffers
 */
class CmdPool {
public:
    using Config = CmdPoolConfig;
    struct State;

    /**
     * base constructor
     * @pre `Core::created()`
     */
    CmdPool(Core const&, Config const&);

    /**
     * constructs and wraps
     * @details calls `CmdPool(Core const&, Config const&)`, `wrap()`
     */
    CmdPool(Core const&, Config const&, State const&);

    /**
     * constructs and creates
     * @details calls `CmdPool(Core const&, Config const&)`, `create()`
     */
    explicit CmdPool(Core const&, Config const&, create_t);

    /**
     * @note calls `optDestroy()`
     */
    ~CmdPool() { optDestroy(); }

    /**
     * wraps the state
     * @pre the pool was created with the config
     * @post `created()`
     * @note calls `optDestroy()`
     */
    void wrap(State const&);

    /**
     * creates the pool
     * @pre the config has a queue family (`!= VK_QUEUE_FAMILY_IGNORED`)
     * @post `created()`
     * @note calls `optDestroy()`
     * @throws cvk::vk_result_exception result of `vkCreateCommandPool()`
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
     * resets every command buffer of the pool to the initial state
     * @param flags `VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT` -> returns the memory of the pool to the system
     * @pre `created()`
     * @pre no command buffer of the pool is pending
     * @post `BaseCmdBuffer::recording()` of every command buffer of the pool is `false`
     * @throws cvk::vk_result_exception result of `vkResetCommandPool()`
     */
    void reset(VkCommandPoolResetFlags flags = 0) const;

    /**
     * counts a command buffer of the pool
     * @details for a released command buffer that the caller keeps, the command buffers count themselves
     */
    void add() const;

    /**
     * stops to count a command buffer of the pool
     * @details for a counted command buffer that the caller frees, the command buffers count themselves
     * @pre `!unused()`
     */
    void remove() const;

    [[nodiscard]] constexpr bool created() const { return static_cast<bool>(_handle); }
    [[nodiscard]] constexpr VkCommandPool get() const { return _handle.get(); }
    [[nodiscard]] constexpr Core const& core() const { return _handle.core(); }
    [[nodiscard]] constexpr Config const& config() const { return _config; }

    /**
     * @return no command buffer of the pool exists
     */
    [[nodiscard]] constexpr bool unused() const { return _buffers == 0; }

    /**
     * @return number of `reset()` calls
     */
    [[nodiscard]] constexpr uint64_t resets() const { return _resets; }

private:
    void forget();

    core_handle<VkCommandPool> _handle;
    Config _config;

    mutable size_t _buffers = 0;
    mutable uint64_t _resets = 0;

public:
    CmdPool(CmdPool const& other) = delete;
    CmdPool(CmdPool&& other) noexcept = default;
    CmdPool& operator=(CmdPool const& other) = delete;
    CmdPool& operator=(CmdPool&& other) noexcept = default;

    static constexpr void debug_check(CmdPool const&);
};
}

namespace cvk {
struct CmdPool::State {
    vk_not_null<VkCommandPool> vkPool;

    /**
     * the raw handles to destroy, in destruction order
     */
    [[nodiscard]] constexpr std::tuple<VkCommandPool> decompose() && { return {vkPool.get()}; }
};
}

namespace cvk {
constexpr void CmdPool::debug_check(CmdPool const& pool) {
    CTH_CRITICAL(!pool.created(), "command pool must be created") {}
}
}
