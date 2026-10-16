#pragma once
#include "cmd_pool.hpp"

#include "cvk/base/device_table.hpp"
#include "cvk/utility/constants.hpp"
#include "cvk/utility/types.hpp"
#include "cvk/utility/unique_handle.hpp"

#include <volk.h>

#include <cth/io/log.hpp>

#include <span>
#include <vector>


namespace cvk {
class Core;

/**
 * @brief VkCommandBuffer wrapper, allocated from its pool, base of @ref PrimaryCmdBuffer and @ref SecondaryCmdBuffer
 * @details the derived classes add the begin of the recording and their commands, no members
 */
class BaseCmdBuffer {
public:
    struct State;

    /**
     * @note calls @ref optDestroy()
     */
    ~BaseCmdBuffer() { optDestroy(); }

    /**
     * @brief wraps the state
     * @pre the pool of the command buffer allocated it with the level
     * @pre the command buffer is not recording
     * @post @ref created()
     * @note calls @ref optDestroy()
     */
    void wrap(State const&);

    /**
     * @brief allocates the command buffer from the pool
     * @pre @ref CmdPool::created()
     * @post @ref created()
     * @note calls @ref optDestroy()
     * @throws cvk::vk_result_exception result of vkAllocateCommandBuffers()
     */
    void create();

    /**
     * @brief frees and forgets
     * @pre @ref created()
     * @pre the gpu does not use the command buffer
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
     * @brief ends the recording
     * @pre @ref recording()
     * @throws cvk::vk_result_exception result of vkEndCommandBuffer()
     */
    void end();

    /**
     * @brief resets the command buffer to the initial state
     * @param flags VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT -> returns the memory to the pool
     * @pre @ref created()
     * @pre the pool has VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
     * @throws cvk::vk_result_exception result of vkResetCommandBuffer()
     */
    void reset(VkCommandBufferResetFlags flags = 0);

    static void destroy(DeviceTable table, VkCommandPool, VkCommandBuffer);

    /**
     * @brief frees the command buffers at once
     */
    static void destroy(DeviceTable table, VkCommandPool, std::span<VkCommandBuffer const> vkBuffers);

    [[nodiscard]] constexpr bool created() const { return static_cast<bool>(_handle); }
    [[nodiscard]] constexpr VkCommandBuffer get() const { return _handle.get(); }
    [[nodiscard]] constexpr Core const& core() const { return _handle.core(); }
    [[nodiscard]] constexpr CmdPool const& pool() const { return _handle.deleter().pool(); }
    [[nodiscard]] constexpr VkCommandBufferLevel level() const { return _level; }
    [[nodiscard]] constexpr bool recording() const { return _recording; }

protected:
    /**
     * @brief base constructor
     * @pre @ref CmdPool::created()
     */
    BaseCmdBuffer(CmdPool const&, VkCommandBufferLevel);

    /**
     * @brief begins the recording
     * @pre @ref created()
     * @pre !@ref recording()
     * @post @ref recording()
     * @throws cvk::vk_result_exception result of vkBeginCommandBuffer()
     */
    void begin(VkCommandBufferBeginInfo const&);

private:
    /**
     * @brief frees the command buffer through its pool and counts the command buffers of the pool
     */
    class pool_deleter {
    public:
        constexpr explicit pool_deleter(CmdPool const& pool) : _pool{&pool} {}

        void operator()(VkCommandBuffer) const;

        [[nodiscard]] constexpr CmdPool const& pool() const { return *_pool; }
        [[nodiscard]] constexpr Core const& core() const { return _pool->core(); }

    private:
        not_null<CmdPool const*> _pool;
    };

    void forget();

    unique_handle<VkCommandBuffer, pool_deleter> _handle;
    VkCommandBufferLevel _level;
    bool _recording = false;

public:
    BaseCmdBuffer(BaseCmdBuffer const& other) = delete;
    BaseCmdBuffer(BaseCmdBuffer&& other) noexcept = default;
    BaseCmdBuffer& operator=(BaseCmdBuffer const& other) = delete;
    BaseCmdBuffer& operator=(BaseCmdBuffer&& other) noexcept = default;

    static constexpr void debug_check(BaseCmdBuffer const&);
};
}

namespace cvk {
struct BaseCmdBuffer::State {
    vk_not_null<VkCommandBuffer> vkBuffer;
};
}

namespace cvk {
constexpr void BaseCmdBuffer::debug_check(BaseCmdBuffer const& buffer) {
    CTH_CRITICAL(!buffer.created(), "command buffer must be created") {}
}
}


namespace cvk {
class SecondaryCmdBuffer;

/**
 * @brief primary command buffer, queues submit it
 */
class PrimaryCmdBuffer : public BaseCmdBuffer {
public:
    /**
     * @brief base constructor
     * @pre @ref CmdPool::created()
     */
    explicit PrimaryCmdBuffer(CmdPool const&);

    /**
     * @brief constructs and wraps
     * @details calls:
     * - @ref PrimaryCmdBuffer(CmdPool const&)
     * - @ref wrap()
     */
    PrimaryCmdBuffer(CmdPool const&, State const&);

    /**
     * @brief constructs and creates
     * @details calls:
     * - @ref PrimaryCmdBuffer(CmdPool const&)
     * - @ref create()
     */
    explicit PrimaryCmdBuffer(CmdPool const&, create_t);

    /**
     * @brief begins the recording
     * @param usage VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, ...
     * @pre @ref created()
     * @pre !@ref recording()
     * @post @ref recording()
     * @throws cvk::vk_result_exception result of vkBeginCommandBuffer()
     */
    void begin(VkCommandBufferUsageFlags usage = 0);

    /**
     * @brief records the secondary command buffers
     * @pre @ref recording()
     * @pre the secondaries are created and not recording
     */
    void execute(std::span<SecondaryCmdBuffer const> secondaries) const;
};
}


namespace cvk {
/**
 * @brief secondary command buffer, primary command buffers execute it
 */
class SecondaryCmdBuffer : public BaseCmdBuffer {
public:
    struct Inheritance;

    /**
     * @brief base constructor
     * @pre @ref CmdPool::created()
     */
    explicit SecondaryCmdBuffer(CmdPool const&);

    /**
     * @brief constructs and wraps
     * @details calls:
     * - @ref SecondaryCmdBuffer(CmdPool const&)
     * - @ref wrap()
     */
    SecondaryCmdBuffer(CmdPool const&, State const&);

    /**
     * @brief constructs and creates
     * @details calls:
     * - @ref SecondaryCmdBuffer(CmdPool const&)
     * - @ref create()
     */
    explicit SecondaryCmdBuffer(CmdPool const&, create_t);

    /**
     * @brief begins the recording for use outside of a rendering
     * @param usage VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, ...
     * @pre @ref created()
     * @pre !@ref recording()
     * @post @ref recording()
     * @throws cvk::vk_result_exception result of vkBeginCommandBuffer()
     */
    void begin(VkCommandBufferUsageFlags usage = 0);

    /**
     * @brief begins the recording for use inside a rendering with the attachments of the inheritance
     * @param usage VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, ...
     * @pre @ref created()
     * @pre !@ref recording()
     * @post @ref recording()
     * @note adds VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT
     * @throws cvk::vk_result_exception result of vkBeginCommandBuffer()
     */
    void begin(Inheritance const&, VkCommandBufferUsageFlags usage = 0);
};
}

namespace cvk {
/**
 * @brief attachments of the rendering that executes the secondary command buffer
 */
struct SecondaryCmdBuffer::Inheritance {
    std::vector<VkFormat> colorFormats{};
    VkFormat depthFormat = VK_FORMAT_UNDEFINED;
    VkFormat stencilFormat = VK_FORMAT_UNDEFINED;
    VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
    uint32_t viewMask = 0;
    VkRenderingFlags flags = 0;

    /**
     * @attention pColorAttachmentFormats points into @ref colorFormats
     */
    [[nodiscard]] constexpr VkCommandBufferInheritanceRenderingInfo renderingInfo() const {
        return VkCommandBufferInheritanceRenderingInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDERING_INFO,
            .pNext = nullptr,
            .flags = flags,
            .viewMask = viewMask,
            .colorAttachmentCount = static_cast<uint32_t>(colorFormats.size()),
            .pColorAttachmentFormats = colorFormats.data(),
            .depthAttachmentFormat = depthFormat,
            .stencilAttachmentFormat = stencilFormat,
            .rasterizationSamples = samples,
        };
    }
};
}
