#pragma once
#include "jvk/sync/resource_states.hpp"

#include "cvk/base/queue/queue_family.hpp"
#include "cvk/data/buffer/buffer.hpp"
#include "cvk/render/cmd/cmd_buffer.hpp"
#include "cvk/render/cmd/cmd_pool.hpp"
#include "cvk/render/cmd/rendering_info.hpp"

#include <volk.h>

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>


namespace cvk {
class Core;
class Image;
}

namespace jvk::dev {
/**
 * cvk command buffer in its own command pool, with the resource states of its recording
 * @details base of @ref PrimaryCmdBuffer and @ref SecondaryCmdBuffer, move only,
 *  owns the pool: one command pool per command buffer, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
 *  the begin of the derived class resets the pool and the resource states,
 *  its buffer commands record the access of the buffer, then call their cvk::BaseCmdBuffer counterpart,
 *  external Vulkan code: @ref use() the state that the code expects, the code, @ref assume() the state it leaves
 * @tparam Base cvk::PrimaryCmdBuffer or cvk::SecondaryCmdBuffer
 * @attention not thread safe: one thread at a time
 */
template<class Base>
class BasicCmdBuffer : public Base {
public:
    /**
     * flags of the own command pool
     */
    static constexpr VkCommandPoolCreateFlags POOL_FLAGS = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;

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
     *  - VK_ATTACHMENT_LOAD_OP_CLEAR or VK_ATTACHMENT_LOAD_OP_DONT_CARE -> @ref ImageAccess::discard
     *  - depth and stencil attachment of one image: one access, discard only if both load ops discard
     * @pre the preconditions of @ref cvk::BaseCmdBuffer::beginRendering()
     * @pre the depth and the stencil attachment of one image have the same layout
     * @attention a discard covers the whole mip level:
     *  the layers outside the view and the area outside the render area lose their contents too
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
    void bindIndexBuffer(cvk::Buffer<T> const& buffer, size_t offset = 0) {
        bindIndexBuffer(static_cast<cvk::BaseBuffer const&>(buffer), cvk::index_type<T>, offset);
    }

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

protected:
    /**
     * creates the own command pool for the queue family, then the command buffer in it
     * @pre @ref cvk::Core::created()
     * @throws cvk::vk_result_exception result of vkCreateCommandPool()
     * @throws cvk::vk_result_exception result of vkAllocateCommandBuffers()
     */
    BasicCmdBuffer(cvk::Core const&, cvk::queue_family_index_t);

    /**
     * frees the command buffer, then destroys its pool
     * @note calls @ref cvk::BaseCmdBuffer::optDestroy()
     */
    ~BasicCmdBuffer() { this->optDestroy(); }

    /**
     * resets the pool and forgets the resource states of the last recording, for the begin of a recording
     * @pre the command buffer is not pending
     * @post !@ref cvk::BaseCmdBuffer::recording()
     * @throws cvk::vk_result_exception result of vkResetCommandPool()
     */
    void reset();

    /**
     * appends the resource states of a later recording, records the barriers before its commands
     * @details calls @ref ResourceStates::append(), then @ref cvk::Barrier::execute()
     * @pre the preconditions of @ref cvk::Barrier::execute()
     */
    void appendStates(ResourceStates const& next);

private:
    /**
     * the pool is constructed before the cvk base, the base gets a reference to it
     */
    explicit BasicCmdBuffer(std::unique_ptr<cvk::CmdPool>);

    /**
     * records the barrier if it is not empty
     */
    void record(cvk::Barrier const&) const;

    ResourceStates _states{};

    // the destructor and the move assignment free the command buffer before they destroy its pool
    std::unique_ptr<cvk::CmdPool> _pool;

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
 * primary command buffer in its own command pool, tracks the resource states of its recording, move only
 * @details records with the commands of @ref cvk::PrimaryCmdBuffer, the tracked commands of
 *  @ref dev::BasicCmdBuffer record the barriers between the accesses
 * @attention not thread safe: one thread at a time
 */
class PrimaryCmdBuffer : public dev::BasicCmdBuffer<cvk::PrimaryCmdBuffer> {
public:
    /**
     * creates the own command pool for the queue family, then the command buffer in it
     * @param queueFamilyIndex of the queues that the command buffer is submitted to
     * @pre @ref cvk::Core::created()
     * @throws cvk::vk_result_exception result of vkCreateCommandPool()
     * @throws cvk::vk_result_exception result of vkAllocateCommandBuffers()
     */
    PrimaryCmdBuffer(cvk::Core const&, cvk::queue_family_index_t queueFamilyIndex);

    /**
     * resets the pool and the resource states, then begins the recording
     * @param usage 0 -> submit any number of times,
     *  VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT -> hint: one submit per recording,
     *  VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT -> pending several times at once
     * @pre the command buffer is not pending
     * @post @ref recording()
     * @throws cvk::vk_result_exception result of vkResetCommandPool()
     * @throws cvk::vk_result_exception result of vkBeginCommandBuffer()
     */
    void begin(VkCommandBufferUsageFlags usage = 0);

    /**
     * executes the secondaries
     * @details ends the secondaries that are recording,
     *  appends their resource states in execute order, the barriers of a secondary before its vkCmdExecuteCommands()
     * @pre no pointer is nullptr
     * @pre no other thread records into the secondaries
     * @pre the preconditions of @ref cvk::PrimaryCmdBuffer::execute()
     * @pre @ref rendering() -> the resource states of the secondaries need no barrier
     * @throws cvk::vk_result_exception result of vkEndCommandBuffer()
     */
    void execute(std::span<SecondaryCmdBuffer* const>);

    /**
     * executes the secondary
     * @details calls @ref execute(std::span<SecondaryCmdBuffer* const>)
     */
    void execute(SecondaryCmdBuffer&);
};
}


namespace jvk {
/**
 * secondary command buffer in its own command pool, tracks the resource states of its recording, move only
 * @details for use outside of a rendering or with an inheritance,
 *  @ref cvk::BaseCmdBuffer::end() is optional: @ref PrimaryCmdBuffer::execute() ends a secondary that is recording
 * @attention not thread safe: one thread at a time
 */
class SecondaryCmdBuffer : public dev::BasicCmdBuffer<cvk::SecondaryCmdBuffer> {
public:
    using Inheritance = cvk::SecondaryCmdBuffer::Inheritance;

    /**
     * creates the own command pool for the queue family, then the command buffer in it
     * @param queueFamilyIndex of the queues that the primaries are submitted to
     * @pre @ref cvk::Core::created()
     * @throws cvk::vk_result_exception result of vkCreateCommandPool()
     * @throws cvk::vk_result_exception result of vkAllocateCommandBuffers()
     */
    SecondaryCmdBuffer(cvk::Core const&, cvk::queue_family_index_t queueFamilyIndex);

    /**
     * resets the pool and the resource states, then begins the recording for use outside of a rendering
     * @param usage see @ref PrimaryCmdBuffer::begin(),
     *  VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT -> several pending primaries execute it
     * @pre the command buffer is not pending, no pending primary executes it
     * @post @ref recording()
     * @throws cvk::vk_result_exception result of vkResetCommandPool()
     * @throws cvk::vk_result_exception result of vkBeginCommandBuffer()
     */
    void begin(VkCommandBufferUsageFlags usage = 0);

    /**
     * resets the pool and the resource states, then begins the recording for use inside the rendering of a primary
     * @param usage see @ref begin(VkCommandBufferUsageFlags)
     * @pre the preconditions of @ref begin(VkCommandBufferUsageFlags)
     * @pre the preconditions of @ref cvk::SecondaryCmdBuffer::begin(Inheritance const&, VkCommandBufferUsageFlags)
     * @post @ref recording()
     * @throws cvk::vk_result_exception result of vkResetCommandPool()
     * @throws cvk::vk_result_exception result of vkBeginCommandBuffer()
     */
    void begin(Inheritance const&, VkCommandBufferUsageFlags usage = 0);
};
}


namespace jvk {
/**
 * jvk command buffer: @ref PrimaryCmdBuffer or @ref SecondaryCmdBuffer
 */
template<class T>
concept cmd_buffer = std::derived_from<T, PrimaryCmdBuffer> || std::derived_from<T, SecondaryCmdBuffer>;
}
