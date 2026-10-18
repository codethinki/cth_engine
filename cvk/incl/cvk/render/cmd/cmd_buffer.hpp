#pragma once
#include "cmd_pool.hpp"
#include "rendering_info.hpp"

#include "cvk/data/buffer/buffer.hpp"

#include "cvk/base/device_table.hpp"
#include "cvk/utility/constants.hpp"
#include "cvk/utility/types.hpp"
#include "cvk/utility/unique_handle.hpp"

#include <volk.h>

#include <cth/io/log.hpp>

#include <optional>
#include <span>
#include <vector>


namespace cvk {
class Core;
class Image;

/**
 * @brief VkCommandBuffer wrapper, allocated from its pool, base of @ref PrimaryCmdBuffer and @ref SecondaryCmdBuffer
 * @details the derived classes add the begin of the recording and their commands, no members,
 *  the destructor is protected: delete through the derived class
 */
class BaseCmdBuffer {
public:
    using vk_handle_t = VkCommandBuffer;
    struct State;

    /**
     * @brief the rendering the command buffer is inside
     */
    struct Rendering {
        /**
         * @brief what a secondary inherits, the flags without VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT
         */
        RenderingInheritance inheritance;

        /**
         * @brief flags of the rendering
         */
        VkRenderingFlags flags = 0;

        /**
         * @brief images of the attachments, empty if inherited
         */
        std::vector<not_null_ptr<Image const>> attachments{};

        /**
         * @brief true -> a secondary that inherits the rendering of its primary
         */
        bool inherited = false;

        /**
         * @brief render area, inherited -> unknown
         */
        VkRect2D area{};
    };

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

    /**
     * @brief begins a dynamic rendering
     * @details std::nullopt @ref RenderingInfo::area -> the extent that fits every attachment
     * @pre @ref recording(), !@ref rendering()
     * @pre the attachment views are created and have one mip level
     * @pre the attachment layouts are the layouts of the views
     * @pre the attachment layouts are attachment layouts (not undefined, transfer, shader read or present)
     * @pre color attachments have color formats, the depth and stencil attachments depth and stencil formats
     * @post @ref rendering()
     */
    void beginRendering(RenderingInfo const&);

    /**
     * @brief ends the dynamic rendering
     * @pre @ref rendering() began with @ref beginRendering()
     */
    void endRendering();

    /**
     * @brief maps the color attachments to the fragment shader outputs
     * @param locations output location of each color attachment (VK_ATTACHMENT_UNUSED => no output)
     * @pre @ref rendering() with one location per color attachment
     */
    void setAttachmentLocations(std::span<uint32_t const> locations) const;

    /**
     * @brief maps the attachments to the input attachments of the fragment shader
     * @param colorIndices input attachment index of each color attachment (VK_ATTACHMENT_UNUSED => no input)
     * @param depthIndex std::nullopt -> the depth attachment is an input attachment without index
     * @param stencilIndex std::nullopt -> the stencil attachment is an input attachment without index
     * @pre @ref rendering() with one index per color attachment
     */
    void setInputAttachmentIndices(
        std::span<uint32_t const> colorIndices,
        std::optional<uint32_t> depthIndex = std::nullopt,
        std::optional<uint32_t> stencilIndex = std::nullopt
    ) const;

    /**
     * @brief sets viewport 0 to the area of the rendering, depth 0 to 1
     * @pre @ref rendering() began with @ref beginRendering()
     */
    void setViewport() const;

    /**
     * @brief sets viewport 0
     * @pre @ref recording()
     */
    void setViewport(VkViewport const&) const;

    /**
     * @brief sets scissor 0 to the area of the rendering
     * @pre @ref rendering() began with @ref beginRendering()
     */
    void setScissor() const;

    /**
     * @brief sets scissor 0
     * @pre @ref recording()
     */
    void setScissor(VkRect2D const&) const;

    /**
     * @brief binds the buffer to the vertex input binding
     * @param offset in bytes
     * @pre @ref recording(), buffer created with VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
     */
    void bindVertexBuffer(uint32_t binding, BaseBuffer const&, size_t offset = 0) const;

    /**
     * @brief binds the buffer to the vertex input binding
     * @param firstVertex offset in elements
     * @details calls @ref bindVertexBuffer(uint32_t, BaseBuffer const&, size_t)
     */
    template<class T>
    void bindVertexBuffer(uint32_t binding, Buffer<T> const& buffer, size_t firstVertex = 0) const {
        bindVertexBuffer(binding, static_cast<BaseBuffer const&>(buffer), firstVertex * sizeof(T));
    }

    /**
     * @brief binds the index buffer
     * @param offset in bytes
     * @pre @ref recording(), buffer created with VK_BUFFER_USAGE_INDEX_BUFFER_BIT
     * @pre offset is a multiple of the index size
     */
    void bindIndexBuffer(BaseBuffer const&, VkIndexType, size_t offset = 0) const;

    /**
     * @brief binds the index buffer, the index type follows from T
     * @param firstIndex offset in elements
     * @details calls @ref bindIndexBuffer(BaseBuffer const&, VkIndexType, size_t)
     */
    template<index_element T>
    void bindIndexBuffer(Buffer<T> const& buffer, size_t firstIndex = 0) const {
        bindIndexBuffer(static_cast<BaseBuffer const&>(buffer), index_type<T>, firstIndex * sizeof(T));
    }

    /**
     * @pre @ref rendering()
     */
    void draw(
        uint32_t vertexCount,
        uint32_t instanceCount = 1,
        uint32_t firstVertex = 0,
        uint32_t firstInstance = 0
    ) const;

    /**
     * @pre @ref rendering(), an index buffer is bound
     */
    void drawIndexed(
        uint32_t indexCount,
        uint32_t instanceCount = 1,
        uint32_t firstIndex = 0,
        int32_t vertexOffset = 0,
        uint32_t firstInstance = 0
    ) const;

    /**
     * @brief draws with the VkDrawIndirectCommands in the buffer
     * @param offset in bytes
     * @pre @ref rendering(), buffer created with VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT
     * @pre the commands lie in the buffer
     */
    void drawIndirect(
        BaseBuffer const&,
        size_t offset = 0,
        uint32_t drawCount = 1,
        uint32_t stride = sizeof(VkDrawIndirectCommand)
    ) const;

    /**
     * @brief draws with the VkDrawIndexedIndirectCommands in the buffer
     * @param offset in bytes
     * @pre @ref rendering(), buffer created with VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, an index buffer is bound
     * @pre the commands lie in the buffer
     */
    void drawIndexedIndirect(
        BaseBuffer const&,
        size_t offset = 0,
        uint32_t drawCount = 1,
        uint32_t stride = sizeof(VkDrawIndexedIndirectCommand)
    ) const;

    static void destroy(DeviceTable table, VkCommandPool, VkCommandBuffer);

    /**
     * @brief frees the command buffers at once
     */
    static void destroy(DeviceTable table, VkCommandPool, std::span<VkCommandBuffer const> vkBuffers);

    [[nodiscard]] constexpr bool created() const { return static_cast<bool>(_handle); }
    [[nodiscard]] constexpr vk_handle_t get() const { return _handle.get(); }
    [[nodiscard]] constexpr Core const& core() const { return _handle.core(); }
    [[nodiscard]] constexpr CmdPool const& pool() const { return _handle.deleter().pool(); }
    [[nodiscard]] constexpr VkCommandBufferLevel level() const { return _level; }

    /**
     * @return recording, a reset of the pool ends it
     */
    [[nodiscard]] constexpr bool recording() const {
        return created() && _recording && _beginResets == pool().resets();
    }

    /**
     * @return the rendering the command buffer records into, nullptr -> outside of a rendering
     */
    [[nodiscard]] constexpr Rendering const* rendering() const {
        return recording() && _rendering.has_value() ? &*_rendering : nullptr;
    }

protected:
    /**
     * @brief base constructor
     * @pre @ref CmdPool::created()
     */
    BaseCmdBuffer(CmdPool const&, VkCommandBufferLevel);

    /**
     * @note calls @ref optDestroy()
     */
    ~BaseCmdBuffer() { optDestroy(); }

    /**
     * @brief begins the recording
     * @param inherited nullptr -> outside of a rendering, else the rendering of the primary
     * @pre @ref created()
     * @pre !@ref recording()
     * @post @ref recording()
     * @throws cvk::vk_result_exception result of vkBeginCommandBuffer()
     */
    void begin(VkCommandBufferBeginInfo const&, RenderingInheritance const* inherited = nullptr);

private:
    /**
     * @brief frees the command buffer through its pool, the pool stops to count it
     */
    class pool_deleter {
    public:
        constexpr explicit pool_deleter(CmdPool const& pool) : _pool{&pool} {}

        void operator()(VkCommandBuffer) const;

        [[nodiscard]] constexpr CmdPool const& pool() const { return *_pool; }
        [[nodiscard]] constexpr Core const& core() const { return _pool->core(); }

    private:
        not_null_ptr<CmdPool const> _pool;
    };

    void forget();

    unique_handle<vk_handle_t, pool_deleter> _handle;
    VkCommandBufferLevel _level;
    bool _recording = false;
    uint64_t _beginResets = 0;
    std::optional<Rendering> _rendering = std::nullopt;

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
    using Inheritance = RenderingInheritance;

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
