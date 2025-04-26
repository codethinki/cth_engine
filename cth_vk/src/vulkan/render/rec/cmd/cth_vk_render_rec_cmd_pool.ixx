module;
#include "lib/volk.hpp"
#include <cth/io/io_log.hpp>

export module cth.vk.render.rec.cmd.pool;

import cth.vk.constants;
import cth.vk.util.types;
import cth.vk.base.device_table;
import cth.vk.base.core;
import cth.vk.res.destruction_queue;


import cth.typ.variadic;
import cth.io.log;
import cth.ptr;

import std;

export namespace cth::vk {

class CmdPool {
public:
    enum BufferType : size_t { BUFFER_TYPE_PRIMARY, BUFFER_TYPE_SECONDARY, BUFFER_TYPES };

    struct Config;
    struct State;

    /**
     * @brief base constructor
     */
    CmdPool(Core const& core, Config const& config);

    /**
     * @brief constructs and wraps
     * @param state passed to @ref wrap()
     * @details calls:
           @ref CmdPool(Core const&, Config const&)
           @ref create()
     */
    CmdPool(Core const& core, Config const& config, State const& state);

    /**
     * @brief constructs and may create
     * @note calls @ref CmdPool(Core const&, Config const&)
     * @details calls @ref create()
     */
    CmdPool(Core const& core, Config const& config, create_t);


    /**
     * @note calls @ref optDestroy()
     */
    ~CmdPool();

    /**
     * @brief wraps the state
     * @note calls @ref optDestroy()
     */
    void wrap(State const& state);

    /**
     * @brief creates the vk resources
     * @note calls @ref optDestroy()
     * @throws cth::vk::result_exception VkResult of vkCreateCommandPool()
     * @throws cth::vk::result_exception VkResult of vkAllocateCommandBuffers()
     */
    void create();

    /**
     * @brief releases ownership and resets
     * @attention all buffers created from this pool must have been returned
     * @attention @ref created() required
     */
    State release();

    /**
     * @brief destroys the vk resources
     * @attention @ref created() required
     *
     */
    void destroy();

    /**
     * @brief if @ref created() calls @ref destroy()
     */
    void optDestroy(this auto&& self);


    VkCommandBuffer newCmdBuffer(BufferType b_type);
    void returnCmdBuffer(BufferType b_type, VkCommandBuffer vk_buffer);


    static void destroy(DeviceTable table, VkCommandPool vk_pool);

private:
    void reset();



    [[nodiscard]] VkCommandPoolCreateInfo createInfo() const;
    /**
     * @brief creates the VkCommandPool
     * @throws cth::vk::result_exception VkResult of vkCreateCommandPool()
     */
    void createPool();

    /**
     * @brief creates the VkCommandBuffers
     * @throws cth::vk::result_exception VkResult of vkAllocateCommandBuffers() (for primary or secondary buffers)
     */
    void alloc();



    cth::not_null<Core const*> _core;

    cth::move_ptr<VkCommandPool_T> _handle = VK_NULL_HANDLE;
    std::array<std::vector<VkCommandBuffer>, BUFFER_TYPES> _buffers;
    std::array<size_t, BUFFER_TYPES> _maxBuffers{};
    VkCommandPoolCreateFlags _flags;

    size_t _maxPrimaryBuffers = 0;
    size_t _maxSecondaryBuffers = 0;
    uint32_t _queueFamilyIndex;

    constexpr static VkCommandBufferLevel to_buffer_level(BufferType type) { return static_cast<VkCommandBufferLevel>(type); }

public:
    [[nodiscard]] bool created() const { return _handle != VK_NULL_HANDLE; }
    [[nodiscard]] auto const& core() const { return *_core; }
    [[nodiscard]] auto queueFamilyIndex() const { return _queueFamilyIndex; }
    /**
     * @return if any buffer depends on this pool -> false 
     */
    [[nodiscard]] bool unused() const {
        return std::ranges::all_of(std::views::zip(_buffers, _maxBuffers),
            [](auto const& pair) { return std::get<0>(pair).size() == std::get<1>(pair); });
    }

    [[nodiscard]] size_t available(BufferType b_type) const { return _buffers[b_type].size(); }

    [[nodiscard]] size_t capacity(BufferType b_type) const { return _maxBuffers[b_type]; }

    CmdPool(CmdPool const& other) = delete;
    CmdPool& operator=(CmdPool const& other) = delete;
    CmdPool(CmdPool&& other) = default;
    CmdPool& operator=(CmdPool&& other) = default;


    static void debug_check(CmdPool const& pool);
    static void debug_check_handle(vk::not_null<VkCommandPool> vk_pool);
    static void debug_check_unused(CmdPool const& pool);
    static void debug_check_buffer_type(BufferType type);
};

}


//Config

export namespace cth::vk {

struct CmdPool::Config {
    size_t maxPrimaryBuffers = 0;
    size_t maxSecondaryBuffers = 0;
    uint32_t queueFamilyIndex = constants::QUEUE_FAMILY_IGNORED;
    VkCommandPoolCreateFlags flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    static Config Default(uint32_t queue_family_index, uint32_t max_primary_buffers, uint32_t max_secondary_buffers) {
        return Config{max_primary_buffers, max_secondary_buffers, queue_family_index};
    }

private:
    friend CmdPool;
};
}

//State

export namespace cth::vk {
struct CmdPool::State {
    vk::not_null<VkCommandPool> vkPool;
    std::vector<vk::not_null<VkCommandBuffer>> vkPrimaryBuffers;
    std::vector<vk::not_null<VkCommandBuffer>> vkSecondaryBuffers;
};
}


//debug_check

export namespace cth::vk {
void CmdPool::debug_check(CmdPool const& pool) {
    CTH_CRITICAL(!pool.created(), "pool must be created") {}
    debug_check_handle(pool._handle.get());
}
void CmdPool::debug_check_handle([[maybe_unused]] vk::not_null<VkCommandPool> vk_pool) {}
void CmdPool::debug_check_unused(CmdPool const& pool) {
    auto const& buffers = pool._buffers;
    auto const& maxBuffers = pool._maxBuffers;

    CTH_CRITICAL(
        std::ranges::fold_left(buffers, 0, [](size_t prev, auto const& vec){ return prev + vec.size(); }) !=
        std::ranges::fold_left(maxBuffers, 0, std::plus<size_t>{}),
        "all buffers must be returned prior to destroying the pool") {
        details->add("missing primary buffers: {}", maxBuffers[BUFFER_TYPE_PRIMARY] - buffers[BUFFER_TYPE_PRIMARY].size());
        details->add("missing secondary buffers: {}", maxBuffers[BUFFER_TYPE_SECONDARY] - buffers[BUFFER_TYPE_SECONDARY].size());
    }
}
void CmdPool::debug_check_buffer_type(BufferType type) {
    CTH_CRITICAL(type > BUFFER_TYPES || type < BUFFER_TYPE_PRIMARY, "type not in range") {}
}
}
