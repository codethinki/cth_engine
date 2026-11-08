#pragma once
#include "jvk/sync/resource_states.hpp"

#include "cvk/base/queue/queue_family.hpp"
#include "cvk/data/buffer/buffer.hpp"
#include "cvk/render/cmd/cmd_buffer.hpp"
#include "cvk/render/cmd/cmd_pool.hpp"
#include "cvk/render/cmd/rendering_info.hpp"

#include <volk.h>

#include <cth/ptr/ref_wrapper.hpp>

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <span>
#include <type_traits>


namespace cvk {
class ComputePipeline;
class Core;
class DescriptorSet;
class GraphicsPipeline;
class Image;
class PipelineLayout;
}

namespace jvk::dev {
template<class Resource, class Access>
concept access_pair = (std::derived_from<Resource, cvk::Image> && std::same_as<Access, ImageAccess>)
    || (std::derived_from<Resource, cvk::BaseBuffer> && std::same_as<Access, BufferAccess>);

template<class... Args>
inline constexpr bool access_pairs = false;

template<>
inline constexpr bool access_pairs<> = true;

template<class Resource, class Access, class... Rest>
inline constexpr bool access_pairs<Resource, Access, Rest...> = access_pair<Resource, Access> && access_pairs<Rest...>;
}

namespace jvk {
/**
 * alternating resources and their accesses, at least two pairs
 * @details pairs: (`cvk::Image` or derived, `ImageAccess`) or (`cvk::BaseBuffer` or derived, `BufferAccess`)
 */
template<class... Args>
concept access_pairs = sizeof...(Args) >= 4 && dev::access_pairs<std::remove_cvref_t<Args>...>;
}

namespace jvk::dev {
//TODO review no wrap(), create(), destroy() or release(): the constructor creates the pool and the command buffer,
// they live with the object
/**
 * base of the jvk command buffers
 * @details
 * - holds the cvk command buffer and owns its command pool
 * - tracks resource states
 * - forwards the untracked cvk commands
 * - external Vulkan code: `use()` before, `assume()` after
 * @tparam Cvk `cvk::PrimaryCmdBuffer` or `cvk::SecondaryCmdBuffer`
 * @attention not thread safe
 */
template<class Cvk>
class BasicCmdBuffer {
public:
    static constexpr VkCommandPoolCreateFlags POOL_FLAGS = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;

    /**
     * ends the recording
     * @see `cvk::BaseCmdBuffer::end()`
     */
    void end() { _cmdBuffer.end(); }

    /**
     * if `recording()` -> calls `end()`
     */
    void optEnd() { _cmdBuffer.optEnd(); }

    /**
     * tracks an access of the image that no tracked command records
     * @details records the barrier from the tracked state
     * @pre `recording()`
     * @pre `insideRendering()` -> only a local read barrier
     * @see `cvk::Barrier::execute()`
     */
    void use(cvk::Image const&, ImageAccess const&);

    /**
     * tracks an access of the buffer that no tracked command records
     * @details records the barrier from the tracked state
     * @pre `recording()`
     * @pre `insideRendering()` -> only a local read barrier
     * @see `cvk::Barrier::execute()`
     */
    void use(cvk::BaseBuffer const&, BufferAccess const&);

    /**
     * tracks the accesses of several resources
     * @details records one barrier for all accesses, in argument order
     * @param args pairs of a resource and its access, see `access_pairs`
     * @pre the preconditions of the single `use()` per pair
     */
    template<class... Args> requires access_pairs<Args...>
    void use(Args const&... args) {
        cvk::Barrier barrier{};
        addAccesses(barrier, args...);
        record(barrier);
    }

    /**
     * sets the tracked state of the mip levels after untracked code
     * @pre `recording()`
     * @see `ResourceStates::assume()`
     */
    void assume(
        cvk::Image const&,
        ImageState const& state,
        uint32_t firstMipLevel = 0,
        uint32_t mipLevels = VK_REMAINING_MIP_LEVELS
    );

    /**
     * sets the tracked state of the buffer after untracked code
     * @pre `recording()`
     * @see `ResourceStates::assume()`
     */
    void assume(cvk::BaseBuffer const&, BufferState const& state);

    /**
     * begins the dynamic rendering
     * @details tracks the accesses of the attachments
     * @details discard: `VK_ATTACHMENT_LOAD_OP_CLEAR` or `VK_ATTACHMENT_LOAD_OP_DONT_CARE` -> the whole mip level
     *  loses its contents, also outside the view and the render area
     * @pre the preconditions of `cvk::BaseCmdBuffer::beginRendering()`
     * @pre the depth and the stencil attachment of one image have the same layout
     */
    void beginRendering(cvk::RenderingInfo const&);

    /**
     * ends the dynamic rendering
     * @details adds the attachment accesses to the last states of the attachments
     * @pre the preconditions of `cvk::BaseCmdBuffer::endRendering()`
     */
    void endRendering();

    /**
     * binds the buffer to the vertex input binding
     * @details tracks the read of the buffer at the bind: write the buffer before the bind
     * @pre the preconditions of `cvk::BaseCmdBuffer::bindVertexBuffer()` and `use()`
     */
    void bindVertexBuffer(uint32_t binding, cvk::BaseBuffer const&, size_t offset = 0);

    /**
     * binds the index buffer
     * @details tracks the read of the buffer at the bind: write the buffer before the bind
     * @pre the preconditions of `cvk::BaseCmdBuffer::bindIndexBuffer()` and `use()`
     */
    void bindIndexBuffer(cvk::BaseBuffer const&, VkIndexType, size_t offset = 0);

    /**
     * binds the index buffer with the index type of `T`
     */
    template<cvk::index_element T>
    void bindIndexBuffer(cvk::Buffer<T> const& buffer, size_t offset = 0) {
        bindIndexBuffer(static_cast<cvk::BaseBuffer const&>(buffer), cvk::index_type<T>, offset);
    }

    /**
     * draws with the `VkDrawIndirectCommands` in the buffer
     * @details tracks the read of the buffer
     * @pre the preconditions of `cvk::BaseCmdBuffer::drawIndirect()` and `use()`
     */
    void drawIndirect(
        cvk::BaseBuffer const&,
        size_t offset = 0,
        uint32_t drawCount = 1,
        uint32_t stride = sizeof(VkDrawIndirectCommand)
    );

    /**
     * draws with the `VkDrawIndexedIndirectCommands` in the buffer
     * @details tracks the read of the buffer
     * @pre the preconditions of `cvk::BaseCmdBuffer::drawIndexedIndirect()` and `use()`
     */
    void drawIndexedIndirect(
        cvk::BaseBuffer const&,
        size_t offset = 0,
        uint32_t drawCount = 1,
        uint32_t stride = sizeof(VkDrawIndexedIndirectCommand)
    );

    /**
     * dispatches with the `VkDispatchIndirectCommand` in the buffer
     * @details tracks the read of the buffer
     * @pre the preconditions of `cvk::BaseCmdBuffer::dispatchIndirect()` and `use()`
     */
    void dispatchIndirect(cvk::BaseBuffer const&, size_t offset = 0);

    //TODO review the forwarders keep the constness of cvk
    /**
     * @see `cvk::BaseCmdBuffer::setAttachmentMapping()`
     */
    void setAttachmentMapping(cvk::AttachmentMapping const& mapping) { _cmdBuffer.setAttachmentMapping(mapping); }

    /**
     * @see `cvk::BaseCmdBuffer::setViewport()`
     */
    void setViewport(VkViewport const& viewport) const { _cmdBuffer.setViewport(viewport); }

    /**
     * @see `cvk::BaseCmdBuffer::setScissor()`
     */
    void setScissor(VkRect2D const& scissor) const { _cmdBuffer.setScissor(scissor); }

    /**
     * @see @ref cvk::BaseCmdBuffer::bindPipeline(cvk::GraphicsPipeline const&)
     */
    void bindPipeline(cvk::GraphicsPipeline const& pipeline) { _cmdBuffer.bindPipeline(pipeline); }

    /**
     * @see @ref cvk::BaseCmdBuffer::bindPipeline(cvk::ComputePipeline const&) const
     */
    void bindPipeline(cvk::ComputePipeline const& pipeline) const { _cmdBuffer.bindPipeline(pipeline); }

    /**
     * @see `cvk::BaseCmdBuffer::bindDescriptorSet()`
     */
    void bindDescriptorSet(
        cvk::DescriptorSet const& set,
        cvk::PipelineLayout const& layout,
        uint32_t setIndex,
        VkPipelineBindPoint bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        std::span<uint32_t const> dynamicOffsets = {}
    ) const {
        _cmdBuffer.bindDescriptorSet(set, layout, setIndex, bindPoint, dynamicOffsets);
    }

    /**
     * @see `cvk::BaseCmdBuffer::pushConstants()`
     */
    template<cvk::push_constant_data T>
    void pushConstants(
        cvk::PipelineLayout const& layout,
        VkShaderStageFlags stages,
        T const& data,
        uint32_t offset = 0
    ) const {
        _cmdBuffer.pushConstants(layout, stages, data, offset);
    }

    /**
     * @see `cvk::BaseCmdBuffer::draw()`
     */
    void draw(
        uint32_t vertexCount,
        uint32_t instanceCount = 1,
        uint32_t firstVertex = 0,
        uint32_t firstInstance = 0
    ) const {
        _cmdBuffer.draw(vertexCount, instanceCount, firstVertex, firstInstance);
    }

    /**
     * @see `cvk::BaseCmdBuffer::drawIndexed()`
     */
    void drawIndexed(
        uint32_t indexCount,
        uint32_t instanceCount = 1,
        uint32_t firstIndex = 0,
        int32_t vertexOffset = 0,
        uint32_t firstInstance = 0
    ) const {
        _cmdBuffer.drawIndexed(indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
    }

    /**
     * @see `cvk::BaseCmdBuffer::dispatch()`
     */
    void dispatch(uint32_t groupCountX, uint32_t groupCountY = 1, uint32_t groupCountZ = 1) const {
        _cmdBuffer.dispatch(groupCountX, groupCountY, groupCountZ);
    }

    [[nodiscard]] constexpr ResourceStates const& states() const { return _states; }

    //TODO review no level() forwarder: the type tells it
    [[nodiscard]] constexpr bool created() const { return _cmdBuffer.created(); }
    [[nodiscard]] constexpr VkCommandBuffer get() const { return _cmdBuffer.get(); }
    [[nodiscard]] constexpr cvk::Core const& core() const { return _cmdBuffer.core(); }
    [[nodiscard]] constexpr cvk::CmdPool const& pool() const { return _cmdBuffer.pool(); }

    /**
     * @see `cvk::BaseCmdBuffer::recording()`
     */
    [[nodiscard]] constexpr bool recording() const { return _cmdBuffer.recording(); }

    /**
     * @see `cvk::BaseCmdBuffer::rendering()`
     */
    [[nodiscard]] constexpr cvk::RenderingInfo const* rendering() const { return _cmdBuffer.rendering(); }

    /**
     * @see `cvk::BaseCmdBuffer::insideRendering()`
     */
    [[nodiscard]] constexpr bool insideRendering() const { return _cmdBuffer.insideRendering(); }

    /**
     * @see `cvk::BaseCmdBuffer::renderingViewMask()`
     */
    [[nodiscard]] constexpr uint32_t renderingViewMask() const { return _cmdBuffer.renderingViewMask(); }

    /**
     * the cvk command buffer
     * @details its commands are untracked: `use()` before, `assume()` after
     * @details records in the current state: no rendering begin or end, attachment mapping or graphics pipeline bind
     */
    constexpr operator Cvk const&() const { return _cmdBuffer; }

protected:
    /**
     * creates the own command pool and the command buffer
     */
    BasicCmdBuffer(cvk::Core const&, cvk::queue_family_index_t);

    // frees the command buffer before the members destroy the pool
    ~BasicCmdBuffer() { _cmdBuffer.optDestroy(); }

    /**
     * resets the pool and the resource states
     * @pre the command buffer is not pending
     * @throws cvk::vk_result_exception result of `vkResetCommandPool()`
     */
    void reset();

    /**
     * appends the resource states of a later recording
     * @details records the barriers for its first accesses
     * @pre the preconditions of `cvk::Barrier::execute()`
     */
    void appendStates(ResourceStates const& next);

    //TODO review protected member: begin() and execute() of the derived classes record into it
    // declared before the pool: the move assignment frees the old command buffer before the old pool
    Cvk _cmdBuffer;

private:
    explicit BasicCmdBuffer(std::unique_ptr<cvk::CmdPool>);

    /**
     * records the barrier if it is not empty
     * @details inside a rendering: adds `VK_DEPENDENCY_BY_REGION_BIT`, with multiview `VK_DEPENDENCY_VIEW_LOCAL_BIT`
     */
    void record(cvk::Barrier&) const;

    template<class Resource, class Access, class... Rest>
    void addAccesses(cvk::Barrier& barrier, Resource const& resource, Access const& access, Rest const&... rest) {
        _states.access(resource, access, barrier);
        if constexpr(sizeof...(Rest) > 0) addAccesses(barrier, rest...);
    }

    ResourceStates _states{};

    std::unique_ptr<cvk::CmdPool> _poolPtr;

public:
    BasicCmdBuffer(BasicCmdBuffer const& other) = delete;
    BasicCmdBuffer(BasicCmdBuffer&& other) noexcept = default;
    BasicCmdBuffer& operator=(BasicCmdBuffer const& other) = delete;
    BasicCmdBuffer& operator=(BasicCmdBuffer&& other) noexcept = default;
};
}

namespace jvk::dev {
extern template class BasicCmdBuffer<cvk::PrimaryCmdBuffer>;
extern template class BasicCmdBuffer<cvk::SecondaryCmdBuffer>;
}


namespace jvk {
class SecondaryCmdBuffer;

/**
 * primary command buffer
 * @details
 * - owns a command pool
 * - tracks resource states
 * @attention not thread safe
 */
class PrimaryCmdBuffer : public dev::BasicCmdBuffer<cvk::PrimaryCmdBuffer> {
public:
    /**
     * creates the own command pool and the command buffer
     * @param queueFamilyIndex of the queues it is submitted to
     * @throws cvk::vk_result_exception result of `vkCreateCommandPool()` or `vkAllocateCommandBuffers()`
     */
    PrimaryCmdBuffer(cvk::Core const&, cvk::queue_family_index_t queueFamilyIndex);

    /**
     * begins the recording
     * @details resets the pool and the resource states
     * @param usage `0` -> any number of submits, `VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT` -> hint: one submit
     * @pre the command buffer is not pending
     * @throws cvk::vk_result_exception result of `vkResetCommandPool()` or `vkBeginCommandBuffer()`
     */
    void begin(VkCommandBufferUsageFlags usage = 0);

    /**
     * executes the secondaries
     * @details calls `optEnd()` on the secondaries
     * @details appends the resource states of the secondaries
     * @pre the preconditions of `cvk::PrimaryCmdBuffer::execute()`
     * @pre `rendering()` -> the secondaries need only local read barriers
     * @throws cvk::vk_result_exception result of `vkEndCommandBuffer()`
     * @see `cvk::Barrier::execute()`
     */
    void execute(std::span<cth::ref_wrapper<SecondaryCmdBuffer> const>);

    //TODO review braced list overload like cvk: libstdc++ 16 has no std::span constructor from a braced list
    /**
     * executes the secondaries of a braced list
     * @see @ref execute(std::span<cth::ref_wrapper<SecondaryCmdBuffer> const>)
     */
    void execute(std::initializer_list<cth::ref_wrapper<SecondaryCmdBuffer>>);

    /**
     * executes the secondary
     * @see @ref execute(std::span<cth::ref_wrapper<SecondaryCmdBuffer> const>)
     */
    void execute(SecondaryCmdBuffer&);

    /**
     * @see `cvk::PrimaryCmdBuffer::inheritance()`
     */
    [[nodiscard]] cvk::RenderingInheritance inheritance() const { return _cmdBuffer.inheritance(); }
};
}


namespace jvk {
/**
 * secondary command buffer
 * @details
 * - owns a command pool
 * - tracks resource states
 * - `end()` is optional, `PrimaryCmdBuffer::execute()` ends it
 * @attention not thread safe
 */
class SecondaryCmdBuffer : public dev::BasicCmdBuffer<cvk::SecondaryCmdBuffer> {
public:
    using Inheritance = cvk::SecondaryCmdBuffer::Inheritance;

    /**
     * creates the own command pool and the command buffer
     * @param queueFamilyIndex of the queues its primaries are submitted to
     * @throws cvk::vk_result_exception result of `vkCreateCommandPool()` or `vkAllocateCommandBuffers()`
     */
    SecondaryCmdBuffer(cvk::Core const&, cvk::queue_family_index_t queueFamilyIndex);

    /**
     * begins the recording outside of a rendering
     * @details resets the pool and the resource states
     * @param usage `VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT` -> several pending primaries execute it
     * @pre no pending primary executes it
     * @throws cvk::vk_result_exception result of `vkResetCommandPool()` or `vkBeginCommandBuffer()`
     */
    void begin(VkCommandBufferUsageFlags usage = 0);

    /**
     * begins the recording inside the rendering of a primary
     * @details resets the pool and the resource states
     * @pre the preconditions of @ref begin(VkCommandBufferUsageFlags)
     * @throws cvk::vk_result_exception result of `vkResetCommandPool()` or `vkBeginCommandBuffer()`
     */
    void begin(Inheritance const&, VkCommandBufferUsageFlags usage = 0);

    /**
     * @see `cvk::SecondaryCmdBuffer::inheritedRendering()`
     */
    [[nodiscard]] constexpr Inheritance const* inheritedRendering() const { return _cmdBuffer.inheritedRendering(); }
};
}


namespace jvk {
/**
 * jvk command buffer
 * @details `PrimaryCmdBuffer` or `SecondaryCmdBuffer`
 */
template<class T>
concept cmd_buffer = std::derived_from<T, PrimaryCmdBuffer> || std::derived_from<T, SecondaryCmdBuffer>;
}
