#pragma once
#include "CthCmdBuffer.hpp"

#include <volk.h>

#include <vector>

#include "vulkan/utility/cth_constants.hpp"
#include "vulkan/utility/cth_vk_types.hpp"


namespace cth::vk {
class DestructionQueue;
class Queue;
class Core;

class CmdBuffer;
class PrimaryCmdBuffer;
class SecondaryCmdBuffer;

template<class T>
concept cmd_buffer_t = type::is_any_of<T, PrimaryCmdBuffer, SecondaryCmdBuffer>;


class CmdPool {
    enum BufferType : size_t { BUFFER_TYPE_PRIMARY, BUFFER_TYPE_SECONDARY, BUFFER_TYPES };

public:
    struct Config;
    struct State;

    /**
     * @brief base constructor
     */
    CmdPool(cth::not_null<Core const*> core, Config const& config);

    /**
     * @brief constructs and wraps
     * @param state passed to @ref wrap()
     * @note calls @ref CmdPool(cth::not_null<Core const*>, Config const&)
     * @note calls @ref create()
     */
    CmdPool(cth::not_null<Core const*> core, Config const& config, State const& state);

    /**
     * @brief constructs and may create
     * @note calls @ref CmdPool(cth::not_null<Core const*>, Config const&)
     * @param create if(true) calls @ref create()
     */
    CmdPool(cth::not_null<Core const*> core, Config const& config, bool create);


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


    static void destroy(vk::not_null<VkDevice> vk_device, VkCommandPool vk_pool);

private:
    void reset();


    template<cmd_buffer_t T>
    VkCommandBuffer newCmdBuffer();

    template<cmd_buffer_t T>
    void returnCmdBuffer(VkCommandBuffer vk_buffer);


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



    friend CmdBuffer;


    cth::not_null<Core const*> _core;

    move_ptr<VkCommandPool_T> _handle = VK_NULL_HANDLE;
    std::array<std::vector<VkCommandBuffer>, BUFFER_TYPES> _buffers;
    std::array<size_t, BUFFER_TYPES> _maxBuffers;
    VkCommandPoolCreateFlags _flags;

    size_t _maxPrimaryBuffers = 0;
    size_t _maxSecondaryBuffers = 0;
    uint32_t _queueFamilyIndex;

    template<class T>
    consteval static auto to_enum() {
        if constexpr(std::same_as<T, PrimaryCmdBuffer>) return BUFFER_TYPE_PRIMARY;
        else if constexpr(std::same_as<T, SecondaryCmdBuffer>) return BUFFER_TYPE_SECONDARY;
        else std::unreachable();
    }
    constexpr static VkCommandBufferLevel to_buffer_level(BufferType type) { return static_cast<VkCommandBufferLevel>(type); }

public:
    [[nodiscard]] bool created() const { return _handle != VK_NULL_HANDLE; }
    [[nodiscard]] auto queueFamilyIndex() const { return _queueFamilyIndex; }
    /**
     * @return if any buffer depends on this pool -> false 
     */
    [[nodiscard]] bool unused() const {
        return std::ranges::all_of(std::views::zip(_buffers, _maxBuffers), [](auto const& pair) { return std::get<0>(pair).size() == std::get<1>(pair); });
    }

    template<cmd_buffer_t T>
    [[nodiscard]] size_t available() const { return _buffers[to_enum<T>()].size(); }

    template<cmd_buffer_t T>
    [[nodiscard]] size_t capacity() const { return _maxBuffers[to_enum<T>()]; }

    CmdPool(CmdPool const& other) = delete;
    CmdPool& operator=(CmdPool const& other) = delete;
    CmdPool(CmdPool&& other) = default;
    CmdPool& operator=(CmdPool&& other) = default;


    static void debug_check(cth::not_null<CmdPool*> pool);
    static void debug_check_handle(vk::not_null<VkCommandPool> vk_pool);
    static void debug_check_unused(cth::not_null<CmdPool*> pool);
};

template void CmdPool::returnCmdBuffer<PrimaryCmdBuffer>(VkCommandBuffer buffer);
template void CmdPool::returnCmdBuffer<SecondaryCmdBuffer>(VkCommandBuffer buffer);
template VkCommandBuffer CmdPool::newCmdBuffer<PrimaryCmdBuffer>();
template VkCommandBuffer CmdPool::newCmdBuffer<SecondaryCmdBuffer>();

}

//Config

namespace cth::vk {

struct CmdPool::Config {
    size_t maxPrimaryBuffers = 0;
    size_t maxSecondaryBuffers = 0;
    uint32_t queueFamilyIndex = constants::QUEUE_FAMILY_IGNORED;
    VkCommandPoolCreateFlags flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    static Config Default(uint32_t queue_family_index, uint32_t max_primary_buffers, uint32_t max_secondary_buffers) {
        return Config{max_primary_buffers, max_secondary_buffers, queue_family_index};
    }
    static Config Default(Queue const& queue, uint32_t max_primary_buffers, uint32_t max_secondary_buffers);

private:
    friend CmdPool;
};
}

//State

namespace cth::vk {
struct CmdPool::State {
    vk::not_null<VkCommandPool> vkPool;
    std::vector<vk::not_null<VkCommandBuffer>> vkPrimaryBuffers;
    std::vector<vk::not_null<VkCommandBuffer>> vkSecondaryBuffers;
};
}


//debug_check

namespace cth::vk {
inline void CmdPool::debug_check(cth::not_null<CmdPool*> pool) {
    CTH_CRITICAL(!pool->created(), "pool must be created") {}
    debug_check_handle(pool->_handle.get());
}
inline void CmdPool::debug_check_handle(vk::not_null<VkCommandPool> vk_pool) {}
inline void CmdPool::debug_check_unused(cth::not_null<CmdPool*> pool) {
    auto const& buffers = pool->_buffers;
    auto const& maxBuffers = pool->_maxBuffers;

    CTH_CRITICAL(
        std::ranges::fold_left(buffers, 0, [](size_t prev, auto const& vec){ return prev + vec.size(); }) !=
        std::ranges::fold_left(maxBuffers, 0, std::plus<size_t>{}),
        "all buffers must be returned prior to destroying the pool") {
        details->add("missing primary buffers: {}", maxBuffers[BUFFER_TYPE_PRIMARY] - buffers[BUFFER_TYPE_PRIMARY].size());
        details->add("missing secondary buffers: {}", maxBuffers[BUFFER_TYPE_SECONDARY] - buffers[BUFFER_TYPE_SECONDARY].size());
    }
}

}
