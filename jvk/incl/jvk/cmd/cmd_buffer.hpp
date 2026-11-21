#pragma once
#include "jvk/data/resource_heap.hpp"
#include "jvk/sync/resource_states.hpp"

#include "cvk/base/queue/queue_family.hpp"
#include "cvk/data/buffer/buffer.hpp"
#include "cvk/render/cmd/cmd_buffer.hpp"
#include "cvk/render/cmd/cmd_pool.hpp"
#include "cvk/render/cmd/rendering_info.hpp"
#include "cvk/utility/constants.hpp"
#include "cvk/utility/types.hpp"

#include <volk.h>

#include <cth/ptr/move_ptr.hpp>
#include <cth/ptr/ref_wrapper.hpp>

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <span>
#include <tuple>
#include <type_traits>


namespace cvk {
class ComputePipeline;
class Core;
class GraphicsPipeline;
class Image;
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
inline constexpr bool access_pairs<Resource, Access, Rest...> = access_pair<Resource, Access>
    && access_pairs<
        Rest...>;
}

namespace jvk {
/**
 * alternating resources and their accesses, at least two pairs
 * @details pairs: (`cvk::Image` or derived, `ImageAccess`) or (`cvk::BaseBuffer` or derived, `BufferAccess`)
 */
template<class... Args>
concept access_pairs = sizeof...(Args) >= 4 && dev::access_pairs<std::remove_cvref_t<Args>...>;
}

namespace jvk {
/**
 * dynamic rendering of a jvk command buffer for the scope, ends it at its end
 * @details
 * - `beginRendering()` of the command buffer returns it
 * - normal end and exception: calls `endRendering()` of the command buffer if the rendering is still open
 * - an explicit `endRendering()` of the command buffer: the end does nothing
 * - a moved-from scope does nothing
 * @tparam CmdBuffer a jvk command buffer
 * @attention a coroutine destroyed while suspended with the scope alive ends the rendering
 */
template<class CmdBuffer>
class scoped_rendering {
public:
    /**
     * @param cmdBuffer that began the rendering
     */
    explicit constexpr
    scoped_rendering(CmdBuffer& cmdBuffer) noexcept : _cmdBufferPtr{&cmdBuffer} {}

    ~scoped_rendering() {
        if(_cmdBufferPtr && _cmdBufferPtr->rendering() != nullptr)
            _cmdBufferPtr->endRendering();
    }

    /**
     * ends the rendering now, the end of the scope does nothing
     * @pre the rendering is open
     */
    void end() { _cmdBufferPtr.release()->endRendering(); }

private:
    cth::move_ptr<CmdBuffer> _cmdBufferPtr;

public:
    scoped_rendering(scoped_rendering const& other) = delete;
    scoped_rendering(scoped_rendering&& other) noexcept = default;
    scoped_rendering& operator=(scoped_rendering const& other) = delete;
    scoped_rendering& operator=(scoped_rendering&& other) = delete;
};
}

namespace jvk::dev {
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
    struct State;

    static constexpr VkCommandPoolCreateFlags POOL_FLAGS = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;

    /**
     * wraps the state
     * @details the resource states start empty
     * @pre `cvk::Core::created()`
     * @pre `State::vkPool` was created with the queue family of the object and `POOL_FLAGS`
     * @pre `State::vkBuffer` is the only command buffer of `State::vkPool`, with the level of `Cvk`
     * @pre the command buffer is not recording
     * @post `created()`
     * @note calls `optDestroy()`
     */
    void wrap(State const&);

    /**
     * creates the own command pool and the command buffer
     * @details the resource states start empty
     * @pre `cvk::Core::created()`
     * @post `created()`
     * @note calls `optDestroy()`
     * @throws cvk::vk_result_exception result of `vkCreateCommandPool()` or `vkAllocateCommandBuffers()`
     */
    void create();

    /**
     * frees the command buffer, destroys the pool and forgets
     * @details drops the resource states
     * @pre `created()`
     * @pre the command buffer is not pending
     */
    void destroy();

    /**
     * if `created()` -> calls `destroy()`
     */
    void optDestroy() {
        if(created())
            destroy();
    }

    /**
     * releases ownership of the command buffer and the pool, returns the state and forgets
     * @details drops the resource states
     * @pre `created()`
     */
    State release();

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
     * tracks a shader read of the sampled image
     * @details `VK_ACCESS_2_SHADER_SAMPLED_READ_BIT` in the `LAYOUT` of the handle
     * @details on the subresource range of its view
     * @param stages that read the image
     * @pre the preconditions of @ref use(cvk::Image const&, ImageAccess const&)
     */
    void use(SampledImage const&, VkPipelineStageFlags2 stages);

    /**
     * tracks a shader access of the storage image
     * @details in the `LAYOUT` of the handle, on the subresource range of its view
     * @param stages that access the image
     * @param access of the stages
     * @pre the preconditions of @ref use(cvk::Image const&, ImageAccess const&)
     */
    void use(
        StorageImage const&,
        VkPipelineStageFlags2 stages,
        VkAccessFlags2 access = VK_ACCESS_2_SHADER_STORAGE_READ_BIT
            | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT
    );

    /**
     * tracks the read of the input attachment in the fragment shader
     * @details `VK_ACCESS_2_INPUT_ATTACHMENT_READ_BIT` in the `LAYOUT` of the handle
     * @details on the subresource range of its view
     * @pre the preconditions of @ref use(cvk::Image const&, ImageAccess const&)
     */
    void use(InputAttachment const&);

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
     * @return scope, calls `endRendering()` at its end, see `scoped_rendering`
     */
    [[nodiscard]] scoped_rendering<BasicCmdBuffer> beginRendering(cvk::RenderingInfo const&);

    /**
     * ends the dynamic rendering
     * @details adds the attachment accesses to the last states of the attachments
     * @pre the preconditions of `cvk::BaseCmdBuffer::endRendering()`
     */
    void endRendering();

    /**
     * binds the buffer to the vertex input binding
     * @details tracks the read of the buffer at the bind: write the buffer before the bind
     * @pre the preconditions of `cvk::BaseCmdBuffer::bindVertices()` and `use()`
     */
    void bindVertices(uint32_t binding, cvk::BaseBuffer const&, size_t offset = 0);

    /**
     * binds the index buffer
     * @details tracks the read of the buffer at the bind: write the buffer before the bind
     * @pre the preconditions of `cvk::BaseCmdBuffer::bindIndices()` and `use()`
     */
    void bindIndices(cvk::BaseBuffer const&, VkIndexType, size_t offset = 0);

    /**
     * binds the index buffer with the index type of `T`
     */
    template<cvk::index_element T>
    void bindIndices(cvk::Buffer<T> const& buffer, size_t offset = 0) {
        bindIndices(static_cast<cvk::BaseBuffer const&>(buffer), cvk::index_type<T>, offset);
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
    void setAttachmentMapping(cvk::AttachmentMapping const& mapping) {
        _cmdBuffer.setAttachmentMapping(mapping);
    }

    /**
     * @see `cvk::BaseCmdBuffer::setViewport()`
     */
    void setViewport(VkViewport const& viewport) const { _cmdBuffer.setViewport(viewport); }

    /**
     * @see `cvk::BaseCmdBuffer::setViewport()`
     */
    void setViewport(VkExtent2D extent, float minDepth = 0.f, float maxDepth = 1.f) const {
        _cmdBuffer.setViewport(extent, minDepth, maxDepth);
    }

    /**
     * @see `cvk::BaseCmdBuffer::setScissor()`
     */
    void setScissor(VkRect2D const& scissor) const { _cmdBuffer.setScissor(scissor); }

    /**
     * @see `cvk::BaseCmdBuffer::setScissor()`
     */
    void setScissor(VkExtent2D extent) const { _cmdBuffer.setScissor(extent); }

    /**
     * @see @ref cvk::BaseCmdBuffer::bind(cvk::GraphicsPipeline const&)
     */
    void bind(cvk::GraphicsPipeline const& pipeline) { _cmdBuffer.bind(pipeline); }

    /**
     * @see @ref cvk::BaseCmdBuffer::bind(cvk::ComputePipeline const&) const
     */
    void bind(cvk::ComputePipeline const& pipeline) const { _cmdBuffer.bind(pipeline); }

    /**
     * @see `cvk::BaseCmdBuffer::pushData()`
     */
    template<cvk::push_constant_data T>
    void pushData(T const& data, uint32_t offset = 0) const { _cmdBuffer.pushData(data, offset); }

    /**
     * @see `cvk::BaseCmdBuffer::draw()`
     */
    void draw(
        uint32_t vertexCount,
        uint32_t instanceCount = 1,
        uint32_t firstVertex = 0,
        uint32_t firstInstance = 0
    ) const { _cmdBuffer.draw(vertexCount, instanceCount, firstVertex, firstInstance); }

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
    [[nodiscard]] constexpr cvk::RenderingInfo const* rendering() const {
        return _cmdBuffer.rendering();
    }

    /**
     * @see `cvk::BaseCmdBuffer::insideRendering()`
     */
    [[nodiscard]] constexpr bool insideRendering() const { return _cmdBuffer.insideRendering(); }

    /**
     * @see `cvk::BaseCmdBuffer::renderingViewMask()`
     */
    [[nodiscard]] constexpr uint32_t renderingViewMask() const {
        return _cmdBuffer.renderingViewMask();
    }

    /**
     * the cvk command buffer
     * @details its commands are untracked: `use()` before, `assume()` after
     * @details records in the current state: no rendering begin or end, attachment mapping or graphics pipeline bind
     */
    constexpr operator Cvk const&() const { return _cmdBuffer; }

protected:
    /**
     * base constructor
     */
    BasicCmdBuffer(cvk::Core const&, cvk::queue_family_index_t);

    /**
     * @note calls `optDestroy()`
     */
    ~BasicCmdBuffer() { optDestroy(); }

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

    void forget();

    template<class Resource, class Access, class... Rest>
    void addAccesses(
        cvk::Barrier& barrier,
        Resource const& resource,
        Access const& access,
        Rest const&... rest
    ) {
        _states.access(resource, access, barrier);
        if constexpr(sizeof...(Rest) > 0)
            addAccesses(barrier, rest...);
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
template<class Cvk>
struct BasicCmdBuffer<Cvk>::State {
    cvk::vk_not_null<VkCommandPool> vkPool; ///< the own pool, it allocated `vkBuffer`
    cvk::vk_not_null<VkCommandBuffer> vkBuffer;

    /**
     * the raw handles to destroy, in destruction order: the command buffer, then the pool
     */
    [[nodiscard]] constexpr std::tuple<std::tuple<VkCommandPool, VkCommandBuffer>, VkCommandPool>
    decompose() && {
        return {std::tuple{vkPool.get(), vkBuffer.get()}, vkPool.get()};
    }
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
     * base constructor
     * @param queueFamilyIndex of the queues it is submitted to
     */
    PrimaryCmdBuffer(cvk::Core const&, cvk::queue_family_index_t queueFamilyIndex);

    /**
     * constructs and wraps
     * @details calls @ref PrimaryCmdBuffer(cvk::Core const&, cvk::queue_family_index_t), `wrap()`
     */
    PrimaryCmdBuffer(cvk::Core const&, cvk::queue_family_index_t queueFamilyIndex, State const&);

    /**
     * constructs and creates
     * @details calls @ref PrimaryCmdBuffer(cvk::Core const&, cvk::queue_family_index_t), `create()`
     */
    explicit PrimaryCmdBuffer(
        cvk::Core const&,
        cvk::queue_family_index_t queueFamilyIndex,
        cvk::create_t
    );

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
     * base constructor
     * @param queueFamilyIndex of the queues its primaries are submitted to
     */
    SecondaryCmdBuffer(cvk::Core const&, cvk::queue_family_index_t queueFamilyIndex);

    /**
     * constructs and wraps
     * @details calls @ref SecondaryCmdBuffer(cvk::Core const&, cvk::queue_family_index_t), `wrap()`
     */
    SecondaryCmdBuffer(cvk::Core const&, cvk::queue_family_index_t queueFamilyIndex, State const&);

    /**
     * constructs and creates
     * @details calls @ref SecondaryCmdBuffer(cvk::Core const&, cvk::queue_family_index_t), `create()`
     */
    explicit SecondaryCmdBuffer(
        cvk::Core const&,
        cvk::queue_family_index_t queueFamilyIndex,
        cvk::create_t
    );

    /**
     * begins the recording outside of a rendering
     * @details resets the pool and the resource states
     * @param usage `VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT` -> several pending primaries execute it
     * @param heaps that the secondary inherits, see `cvk::SecondaryCmdBuffer::begin()`
     * @pre no pending primary executes it
     * @throws cvk::vk_result_exception result of `vkResetCommandPool()` or `vkBeginCommandBuffer()`
     */
    void begin(VkCommandBufferUsageFlags usage = 0, cvk::HeapInheritance const& heaps = {});

    /**
     * begins the recording inside the rendering of a primary
     * @details resets the pool and the resource states
     * @param usage see @ref begin(VkCommandBufferUsageFlags, cvk::HeapInheritance const&)
     * @param heaps that the secondary inherits, see `cvk::SecondaryCmdBuffer::begin()`
     * @pre the preconditions of @ref begin(VkCommandBufferUsageFlags, cvk::HeapInheritance const&)
     * @throws cvk::vk_result_exception result of `vkResetCommandPool()` or `vkBeginCommandBuffer()`
     */
    void begin(
        Inheritance const&,
        VkCommandBufferUsageFlags usage = 0,
        cvk::HeapInheritance const& heaps = {}
    );

    /**
     * @see `cvk::SecondaryCmdBuffer::inheritedRendering()`
     */
    [[nodiscard]] constexpr Inheritance const* inheritedRendering() const {
        return _cmdBuffer.inheritedRendering();
    }
};
}


namespace jvk {
/**
 * jvk command buffer
 * @details `PrimaryCmdBuffer` or `SecondaryCmdBuffer`
 */
template<class T>
concept cmd_buffer = std::derived_from<T, PrimaryCmdBuffer> || std::derived_from<T,
    SecondaryCmdBuffer>;
}
