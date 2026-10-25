#pragma once
#include "jvk/sync/resource_states.hpp"

#include "cvk/data/buffer/buffer.hpp"
#include "cvk/render/cmd/cmd_buffer.hpp"
#include "cvk/render/cmd/rendering_info.hpp"

#include <volk.h>

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <type_traits>
#include <vector>


namespace cvk {
class Image;
}

namespace jvk {
class PrimaryCmdBuffer;
}

namespace jvk::dev {
/**
 * cvk command buffer with tasks and the resource states of its recording
 * @details base of @ref PrimaryCmdBuffer and @ref SecondaryCmdBuffer,
 *  the tasks run after the gpu finished the submit of the command buffer,
 *  its buffer commands record the access of the buffer, then call their cvk::BaseCmdBuffer counterpart,
 *  external Vulkan code: @ref use() the state that the code expects, the code, @ref assume() the state it leaves
 * @tparam Base cvk::PrimaryCmdBuffer or cvk::SecondaryCmdBuffer
 * @attention not thread safe: one thread at a time
 */
template<class Base>
class BasicCmdBuffer : public Base {
public:
    using Base::Base;

    /**
     * keeps the object until the gpu finished the submit of the command buffer, then destroys it
     * @details calls @ref add()
     */
    template<class T> requires std::constructible_from<std::decay_t<T>, T>
    void retain(T&& object);

    /**
     * adds a task that runs after the gpu finished the submit of the command buffer
     */
    void add(std::move_only_function<void()> task);

    /**
     * accesses the image now, records the barrier from the tracked state
     * @details e.g. a shader access through a descriptor, also bindless,
     *  calls @ref ResourceStates::access()
     * @pre @ref cvk::BaseCmdBuffer::recording()
     * @pre @ref cvk::Image::created()
     * @pre @ref cvk::BaseCmdBuffer::rendering() -> the barrier follows the rules of @ref cvk::Barrier::execute()
     *  inside a rendering, use the images of a rendering before its begin
     */
    void use(cvk::Image const&, ImageAccess const&);

    /**
     * accesses the buffer now, records the barrier from the tracked state
     * @details e.g. a shader access through a descriptor, also bindless or a device address,
     *  calls @ref ResourceStates::access()
     * @pre @ref cvk::BaseCmdBuffer::recording()
     * @pre @ref cvk::BaseBuffer::created()
     * @pre @ref cvk::BaseCmdBuffer::rendering() -> the barrier follows the rules of @ref cvk::Barrier::execute()
     *  inside a rendering, use the buffers of a rendering before its begin
     */
    void use(cvk::BaseBuffer const&, BufferAccess const&);

    /**
     * sets the tracked state of the mip levels, records no barrier
     * @details for barriers that the caller recorded and for external Vulkan code,
     *  calls @ref ResourceStates::assume(cvk::Image const&, ImageState const&, uint32_t, uint32_t)
     * @param state of the last access of that code, unknown -> VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT with
     *  VK_ACCESS_2_MEMORY_WRITE_BIT
     * @param mipLevels (VK_REMAINING_MIP_LEVELS => to the last mip level)
     * @pre @ref cvk::BaseCmdBuffer::recording()
     */
    void assume(
        cvk::Image const&,
        ImageState const& state,
        uint32_t firstMipLevel = 0,
        uint32_t mipLevels = VK_REMAINING_MIP_LEVELS
    );

    /**
     * sets the tracked state of the buffer, records no barrier
     * @details for barriers that the caller recorded and for external Vulkan code,
     *  calls @ref ResourceStates::assume(cvk::BaseBuffer const&, BufferState const&)
     * @param state of the last access of that code, unknown -> VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT with
     *  VK_ACCESS_2_MEMORY_WRITE_BIT
     * @pre @ref cvk::BaseCmdBuffer::recording()
     */
    void assume(cvk::BaseBuffer const&, BufferState const& state);

    /**
     * records the accesses of the attachments, then begins the dynamic rendering
     * @details access per attachment, the mip level of its view in the layout of the attachment:
     *  - color: color attachment output stage, write, read with VK_ATTACHMENT_LOAD_OP_LOAD
     *  - depth, stencil: early and late fragment tests, read, write unless the layout is read only for the aspect
     *  - resolve: color attachment output stage, color attachment write, in the resolve layout
     * @pre the preconditions of @ref cvk::BaseCmdBuffer::beginRendering()
     * @pre the depth and the stencil attachment of one image have the same layout
     */
    void beginRendering(cvk::RenderingInfo const&);

    /**
     * records the access of the buffer, then binds it to the vertex input binding
     * @details access: vertex attribute input stage, vertex attribute read,
     *  it counts at the bind: write the buffer before the bind
     * @pre the preconditions of @ref cvk::BaseCmdBuffer::bindVertexBuffer() and of @ref use()
     */
    void bindVertexBuffer(uint32_t binding, cvk::BaseBuffer const&, size_t offset = 0);

    /**
     * records the access of the buffer, then binds it as the index buffer
     * @details access: index input stage, index read,
     *  it counts at the bind: write the buffer before the bind
     * @pre the preconditions of @ref cvk::BaseCmdBuffer::bindIndexBuffer() and of @ref use()
     */
    void bindIndexBuffer(cvk::BaseBuffer const&, VkIndexType, size_t offset = 0);

    /**
     * records the access of the buffer, then binds it as the index buffer, the index type follows from T
     * @details calls @ref bindIndexBuffer(cvk::BaseBuffer const&, VkIndexType, size_t)
     */
    template<cvk::index_element T>
    void bindIndexBuffer(cvk::Buffer<T> const&, size_t offset = 0);

    /**
     * records the access of the buffer, then draws with its VkDrawIndirectCommands
     * @details access: draw indirect stage, indirect command read
     * @pre the preconditions of @ref cvk::BaseCmdBuffer::drawIndirect() and of @ref use()
     */
    void drawIndirect(
        cvk::BaseBuffer const&,
        size_t offset = 0,
        uint32_t drawCount = 1,
        uint32_t stride = sizeof(VkDrawIndirectCommand)
    );

    /**
     * records the access of the buffer, then draws with its VkDrawIndexedIndirectCommands
     * @details access: draw indirect stage, indirect command read
     * @pre the preconditions of @ref cvk::BaseCmdBuffer::drawIndexedIndirect() and of @ref use()
     */
    void drawIndexedIndirect(
        cvk::BaseBuffer const&,
        size_t offset = 0,
        uint32_t drawCount = 1,
        uint32_t stride = sizeof(VkDrawIndexedIndirectCommand)
    );

    /**
     * records the access of the buffer, then dispatches with its VkDispatchIndirectCommand
     * @details access: draw indirect stage, indirect command read
     * @pre the preconditions of @ref cvk::BaseCmdBuffer::dispatchIndirect() and of @ref use()
     */
    void dispatchIndirect(cvk::BaseBuffer const&, size_t offset = 0);

    /**
     * @return resource states of the recording
     */
    [[nodiscard]] constexpr ResourceStates const& states() const { return _states; }

private:
    friend class jvk::PrimaryCmdBuffer;

    std::vector<std::move_only_function<void()>> _tasks{};
    ResourceStates _states{};
};
}

namespace jvk::dev {
extern template class BasicCmdBuffer<cvk::PrimaryCmdBuffer>;
extern template class BasicCmdBuffer<cvk::SecondaryCmdBuffer>;
}


namespace jvk {
class SecondaryCmdBuffer;

/**
 * cvk primary command buffer with tasks that run after the gpu finished its submit
 * @details records with the commands of @ref cvk::PrimaryCmdBuffer,
 *  gives its allocator back when its recording ends,
 *  the submit runs the tasks on a scheduler worker after its fence
 * @attention not thread safe: one thread at a time
 */
class PrimaryCmdBuffer : public dev::BasicCmdBuffer<cvk::PrimaryCmdBuffer> {
public:
    using BasicCmdBuffer::BasicCmdBuffer;

    /**
     * begins the recording with empty resource states
     * @details calls @ref cvk::PrimaryCmdBuffer::begin()
     * @pre the submit or the primary of the last recording took its tasks
     */
    void begin(VkCommandBufferUsageFlags usage = 0);

    /**
     * executes the secondaries
     * @details ends the secondaries that did not end, takes over their tasks, gives back their allocators,
     *  appends their resource states in execute order, the barriers of a secondary before its vkCmdExecuteCommands()
     * @pre no thread records into the secondaries
     * @pre the preconditions of @ref cvk::PrimaryCmdBuffer::execute()
     * @pre @ref rendering() -> the resource states of the secondaries need no barrier
     */
    void execute(std::span<SecondaryCmdBuffer* const>);
};
}


namespace jvk {
/**
 * cvk secondary command buffer with tasks, the primary that executes it takes them over
 * @attention not thread safe: one thread at a time
 */
class SecondaryCmdBuffer : public dev::BasicCmdBuffer<cvk::SecondaryCmdBuffer> {
public:
    using BasicCmdBuffer::BasicCmdBuffer;

    /**
     * begins the recording with empty resource states, for use outside of a rendering
     * @details calls @ref cvk::SecondaryCmdBuffer::begin(VkCommandBufferUsageFlags)
     * @pre the primary of the last recording took its tasks
     */
    void begin(VkCommandBufferUsageFlags usage = 0);

    /**
     * begins the recording with empty resource states, for use inside the rendering of a primary
     * @details calls @ref cvk::SecondaryCmdBuffer::begin(Inheritance const&, VkCommandBufferUsageFlags)
     * @pre the primary of the last recording took its tasks
     */
    void begin(Inheritance const&, VkCommandBufferUsageFlags usage = 0);

    /**
     * ends the recording, gives the allocator back
     * @details optional, @ref PrimaryCmdBuffer::execute() ends a secondary that did not end
     * @pre @ref recording()
     * @throws cvk::vk_result_exception result of vkEndCommandBuffer()
     */
    void end();
};
}


namespace jvk {
/**
 * jvk command buffer: @ref PrimaryCmdBuffer or @ref SecondaryCmdBuffer
 */
template<class T>
concept cmd_buffer = std::derived_from<T, PrimaryCmdBuffer> || std::derived_from<T, SecondaryCmdBuffer>;
}
