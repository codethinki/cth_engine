#pragma once
#include "cmd_pool_config.hpp"

#include "cvk/base/device_table.hpp"
#include "cvk/utility/constants.hpp"
#include "cvk/utility/handle/core_handle.hpp"
#include "cvk/utility/types.hpp"

#include <volk.h>

#include <cth/io/log.hpp>


namespace cvk {
class Core;

/**
 * @brief VkCommandPool wrapper, counts its command buffers and its resets
 * @details the pool and its command buffers are used by one thread at a time
 */
class CmdPool {
public:
    using vk_handle_t = VkCommandPool;
    using Config = CmdPoolConfig;
    struct State;

    /**
     * @brief base constructor
     * @pre @ref Core::created()
     */
    CmdPool(Core const&, Config const&);

    /**
     * @brief constructs and wraps
     * @details calls:
     * - @ref CmdPool(Core const&, Config const&)
     * - @ref wrap()
     */
    CmdPool(Core const&, Config const&, State const&);

    /**
     * @brief constructs and creates
     * @details calls:
     * - @ref CmdPool(Core const&, Config const&)
     * - @ref create()
     */
    explicit CmdPool(Core const&, Config const&, create_t);

    /**
     * @note calls @ref optDestroy()
     */
    ~CmdPool() { optDestroy(); }

    /**
     * @brief wraps the state
     * @pre the pool was created with the config
     * @post @ref created()
     * @note calls @ref optDestroy()
     */
    void wrap(State const&);

    /**
     * @brief creates the pool
     * @pre the config has a queue family (!= VK_QUEUE_FAMILY_IGNORED)
     * @post @ref created()
     * @note calls @ref optDestroy()
     * @throws cvk::vk_result_exception result of vkCreateCommandPool()
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
     * @brief resets every command buffer of the pool to the initial state
     * @param flags VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT -> returns the memory of the pool to the system
     * @pre @ref created()
     * @pre no command buffer of the pool is pending
     * @post @ref BaseCmdBuffer::recording() of every command buffer of the pool is false
     * @throws cvk::vk_result_exception result of vkResetCommandPool()
     */
    void reset(VkCommandPoolResetFlags flags = 0) const;

    /**
     * @brief counts a command buffer of the pool
     * @details @ref BaseCmdBuffer::create() and @ref BaseCmdBuffer::wrap() call it,
     *  code that keeps a released command buffer of the pool may call it too
     */
    void add() const;

    /**
     * @brief stops to count a command buffer of the pool
     * @details @ref BaseCmdBuffer::destroy() and @ref BaseCmdBuffer::release() call it,
     *  code that frees a counted command buffer itself calls it too
     * @pre !@ref unused()
     */
    void remove() const;

    static void destroy(DeviceTable table, VkCommandPool);

    [[nodiscard]] constexpr bool created() const { return static_cast<bool>(_handle); }
    [[nodiscard]] constexpr vk_handle_t get() const { return _handle.get(); }
    [[nodiscard]] constexpr Core const& core() const { return _handle.core(); }
    [[nodiscard]] constexpr Config const& config() const { return _config; }

    /**
     * @return no command buffer of the pool exists
     */
    [[nodiscard]] constexpr bool unused() const { return _buffers == 0; }

    /**
     * @return number of @ref reset() calls
     */
    [[nodiscard]] constexpr uint64_t resets() const { return _resets; }

private:
    void forget();

    core_handle<CmdPool> _handle;
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
};
}

namespace cvk {
constexpr void CmdPool::debug_check(CmdPool const& pool) {
    CTH_CRITICAL(!pool.created(), "command pool must be created") {}
}
}
