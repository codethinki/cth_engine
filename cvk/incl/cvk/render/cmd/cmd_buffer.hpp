#pragma once
#include "cmd_pool.hpp"
#include "rendering_info.hpp"

#include "cvk/data/buffer/buffer.hpp"

#include "cvk/base/device_table.hpp"
#include "cvk/utility/constants.hpp"
#include "cvk/utility/handle/unique_handle.hpp"
#include "cvk/utility/types.hpp"

#include <volk.h>

#include <cth/io/log.hpp>
#include <cth/meta/ranges.hpp>

#include <cstddef>
#include <optional>
#include <ranges>
#include <span>
#include <type_traits>
#include <vector>


namespace cvk {
class ComputePipeline;
class Core;
class DescriptorSet;
class GraphicsPipeline;
class PipelineLayout;

/**
 * @brief contiguous range of trivially copyable elements
 */
template<class T>
concept push_constant_range = std::ranges::contiguous_range<T const&>
    && cth::mta::range_over_cpt<T, CPT(cth::mta::trivially_copyable)>;

/**
 * @brief @ref push_constant_range or trivially copyable value that is no pointer
 */
template<class T>
concept push_constant_data = push_constant_range<T> || (cth::mta::trivially_copyable<T> && !std::is_pointer_v<T>);

/**
 * @brief VkCommandBuffer wrapper, allocated from its pool, base of @ref PrimaryCmdBuffer and @ref SecondaryCmdBuffer
 * @details the derived classes add the begin of the recording and their commands,
 *  the destructor is protected: delete through the derived class
 */
class BaseCmdBuffer {
public:
    using vk_handle_t = VkCommandBuffer;
    struct State;

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
     * @pre @ref recording(), outside of a rendering
     * @pre the attachment views are created and have one mip level
     * @pre the attachment layouts are the layouts of the views
     * @pre the attachment layouts are attachment layouts (not undefined, transfer, shader read or present)
     * @pre color attachments have color formats, the depth and stencil attachments depth and stencil formats
     * @pre @ref AttachmentMapping::debug_check() passes for the color attachments
     * @post @ref rendering() is the info with the area
     * @note inline contents -> sets viewport 0 and scissor 0 to the area, depth 0 to 1
     * @note a mapping other than the default -> sets it after the begin
     */
    void beginRendering(RenderingInfo const&);

    /**
     * @brief ends the dynamic rendering
     * @pre @ref rendering()
     */
    void endRendering();

    /**
     * @brief maps the attachments of the rendering to the fragment shader outputs and input attachments
     * @details the mapping holds until the next call or the end of the rendering
     * @pre @ref rendering()
     * @pre @ref AttachmentMapping::debug_check() passes for the color attachments of the rendering
     * @post @ref rendering() has the mapping
     */
    void setAttachmentMapping(AttachmentMapping const&);

    /**
     * @brief sets viewport 0
     * @pre @ref recording()
     */
    void setViewport(VkViewport const&) const;

    /**
     * @brief sets scissor 0
     * @pre @ref recording()
     */
    void setScissor(VkRect2D const&) const;

    /**
     * @brief binds the pipeline to the graphics bind point
     * @details @ref rendering() -> sets the attachment mapping of the pipeline if the current one differs
     * @pre @ref recording(), @ref GraphicsPipeline::created()
     * @pre a secondary inside the rendering of a primary -> the attachment mapping of the pipeline == the inherited one
     * @note outside of a rendering: sets no attachment mapping, the draws need the default of @ref beginRendering()
     */
    void bindPipeline(GraphicsPipeline const&);

    /**
     * @brief binds the pipeline to the compute bind point
     * @pre @ref recording(), @ref ComputePipeline::created()
     */
    void bindPipeline(ComputePipeline const&) const;

    /**
     * @brief binds the set to the set number of the pipeline layout
     * @param setIndex set number in the pipeline layout
     * @param dynamicOffsets one per dynamic buffer descriptor of the set
     * @pre @ref recording(), @ref DescriptorSet::created()
     * @pre the set layout of setIndex in the pipeline layout is compatible with the layout of the set
     */
    void bindDescriptorSet(
        DescriptorSet const&,
        PipelineLayout const&,
        uint32_t setIndex,
        VkPipelineBindPoint bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        std::span<uint32_t const> dynamicOffsets = {}
    ) const;

    /**
     * @brief records push constants
     * @param data @ref push_constant_range -> the bytes of the elements, else the bytes of the value
     * @param offset in bytes
     * @pre @ref recording(), @ref PipelineLayout::created()
     * @pre data has bytes, offset and the byte count are multiples of 4
     * @pre the push constant ranges of the layout cover the bytes for the stages
     */
    template<push_constant_data T>
    void pushConstants(
        PipelineLayout const& layout,
        VkShaderStageFlags stages,
        T const& data,
        uint32_t offset = 0
    ) const {
        if constexpr(push_constant_range<T>) pushConstantBytes(layout, stages, std::as_bytes(std::span{data}), offset);
        else pushConstantBytes(layout, stages, std::as_bytes(std::span{&data, 1}), offset);
    }

    /**
     * @brief binds the buffer to the vertex input binding
     * @param offset in bytes
     * @pre @ref recording(), buffer created with VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
     */
    void bindVertexBuffer(uint32_t binding, BaseBuffer const&, size_t offset = 0) const;

    /**
     * @brief binds the index buffer
     * @param offset in bytes
     * @pre @ref recording(), buffer created with VK_BUFFER_USAGE_INDEX_BUFFER_BIT
     * @pre offset is a multiple of the index size
     */
    void bindIndexBuffer(BaseBuffer const&, VkIndexType, size_t offset = 0) const;

    /**
     * @brief binds the index buffer, the index type follows from T
     * @param offset in bytes
     * @details calls @ref bindIndexBuffer(BaseBuffer const&, VkIndexType, size_t)
     */
    template<index_element T>
    void bindIndexBuffer(Buffer<T> const& buffer, size_t offset = 0) const {
        bindIndexBuffer(static_cast<BaseBuffer const&>(buffer), index_type<T>, offset);
    }

    /**
     * @pre @ref rendering() or a secondary inside the rendering of a primary
     * @pre @ref rendering() -> without VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT
     */
    void draw(
        uint32_t vertexCount,
        uint32_t instanceCount = 1,
        uint32_t firstVertex = 0,
        uint32_t firstInstance = 0
    ) const;

    /**
     * @pre the preconditions of @ref draw(), an index buffer is bound
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
     * @pre the preconditions of @ref draw(), buffer created with VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT
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
     * @pre the preconditions of @ref draw(), buffer created with VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT
     * @pre an index buffer is bound
     * @pre the commands lie in the buffer
     */
    void drawIndexedIndirect(
        BaseBuffer const&,
        size_t offset = 0,
        uint32_t drawCount = 1,
        uint32_t stride = sizeof(VkDrawIndexedIndirectCommand)
    ) const;

    /**
     * @brief records a dispatch of the bound compute pipeline
     * @param groupCountX work groups in x, the same for y and z
     * @pre @ref recording(), outside of a rendering
     */
    void dispatch(uint32_t groupCountX, uint32_t groupCountY = 1, uint32_t groupCountZ = 1) const;

    /**
     * @brief dispatches with the VkDispatchIndirectCommand in the buffer
     * @param offset in bytes
     * @pre the preconditions of @ref dispatch(), buffer created with VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT
     * @pre the command lies in the buffer
     */
    void dispatchIndirect(BaseBuffer const&, size_t offset = 0) const;

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
     * @return info of the rendering the command buffer began, with the area and the current attachment mapping
     * @return nullptr -> no rendering of the command buffer
     */
    [[nodiscard]] constexpr RenderingInfo const* rendering() const {
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
     * @pre @ref created()
     * @pre !@ref recording()
     * @post @ref recording()
     * @throws cvk::vk_result_exception result of vkBeginCommandBuffer()
     */
    void begin(VkCommandBufferBeginInfo const&);

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

    /**
     * @brief records the attachment mapping of @ref rendering()
     */
    void recordAttachmentMapping() const;

    /**
     * @brief records the bytes as push constants
     */
    void pushConstantBytes(
        PipelineLayout const&,
        VkShaderStageFlags stages,
        std::span<std::byte const> data,
        uint32_t offset
    ) const;

    void forget();

    unique_handle<vk_handle_t, pool_deleter> _handle;
    VkCommandBufferLevel _level;
    bool _recording = false;
    uint64_t _beginResets = 0;
    std::optional<RenderingInfo> _rendering = std::nullopt;

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
     * @pre @ref rendering() -> VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT
     * @pre @ref rendering() -> the secondaries continue it with the current attachment mapping
     * @pre outside of a rendering -> the secondaries continue no rendering
     */
    void execute(std::span<SecondaryCmdBuffer const> secondaries) const;

    /**
     * @brief what a secondary inherits from the rendering
     * @details the formats and samples of the attachments, the flags without
     *  VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT, the current attachment mapping
     * @pre @ref rendering()
     */
    [[nodiscard]] RenderingInheritance inheritance() const;
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
     * @brief begins the recording for use inside the rendering of a primary
     * @param usage VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, ...
     * @pre @ref created()
     * @pre !@ref recording()
     * @pre @ref AttachmentMapping::debug_check() passes for the color formats
     * @post @ref recording()
     * @note adds VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT
     * @throws cvk::vk_result_exception result of vkBeginCommandBuffer()
     */
    void begin(Inheritance const&, VkCommandBufferUsageFlags usage = 0);
};
}
