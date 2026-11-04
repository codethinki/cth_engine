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
 * cvk command buffer in its own command pool, tracks the resource states of its recording, move only
 * @details external Vulkan code: @ref use() before, @ref assume() after
 * @tparam Base cvk::PrimaryCmdBuffer or cvk::SecondaryCmdBuffer
 * @attention not thread safe
 */
template<class Base>
class BasicCmdBuffer : public Base {
public:
    static constexpr VkCommandPoolCreateFlags POOL_FLAGS = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;

    /**
     * accesses the image now, records the barrier from its tracked state
     * @pre @ref cvk::BaseCmdBuffer::recording()
     * @pre @ref cvk::BaseCmdBuffer::rendering() -> the rules of @ref cvk::Barrier::execute()
     */
    void use(cvk::Image const&, ImageAccess const&);

    /**
     * accesses the buffer now, records the barrier from its tracked state
     * @pre @ref cvk::BaseCmdBuffer::recording()
     * @pre @ref cvk::BaseCmdBuffer::rendering() -> the rules of @ref cvk::Barrier::execute()
     */
    void use(cvk::BaseBuffer const&, BufferAccess const&);

    /**
     * sets the tracked state of the mip levels without a barrier, see @ref ResourceStates::assume()
     * @pre @ref cvk::BaseCmdBuffer::recording()
     */
    void assume(
        cvk::Image const&,
        ImageState const& state,
        uint32_t firstMipLevel = 0,
        uint32_t mipLevels = VK_REMAINING_MIP_LEVELS
    );

    /**
     * sets the tracked state of the buffer without a barrier, see @ref ResourceStates::assume()
     * @pre @ref cvk::BaseCmdBuffer::recording()
     */
    void assume(cvk::BaseBuffer const&, BufferState const& state);

    /**
     * records the accesses of the attachments, then begins the dynamic rendering
     * @details VK_ATTACHMENT_LOAD_OP_CLEAR or VK_ATTACHMENT_LOAD_OP_DONT_CARE -> @ref ImageAccess::discard
     * @pre the preconditions of @ref cvk::BaseCmdBuffer::beginRendering()
     * @pre depth and stencil attachment of one image -> the same layout
     * @attention a discard covers the whole mip level, also outside the view and the render area
     */
    void beginRendering(cvk::RenderingInfo const&);

    /**
     * records the access of the buffer, then binds it
     * @pre the preconditions of @ref cvk::BaseCmdBuffer::bindVertexBuffer() and @ref use()
     */
    void bindVertexBuffer(uint32_t binding, cvk::BaseBuffer const&, size_t offset = 0);

    /**
     * records the access of the buffer, then binds it
     * @pre the preconditions of @ref cvk::BaseCmdBuffer::bindIndexBuffer() and @ref use()
     */
    void bindIndexBuffer(cvk::BaseBuffer const&, VkIndexType, size_t offset = 0);

    /**
     * records the access of the buffer, then binds it with the index type of T
     */
    template<cvk::index_element T>
    void bindIndexBuffer(cvk::Buffer<T> const& buffer, size_t offset = 0) {
        bindIndexBuffer(static_cast<cvk::BaseBuffer const&>(buffer), cvk::index_type<T>, offset);
    }

    /**
     * records the access of the buffer, then draws with its commands
     * @pre the preconditions of @ref cvk::BaseCmdBuffer::drawIndirect() and @ref use()
     */
    void drawIndirect(
        cvk::BaseBuffer const&,
        size_t offset = 0,
        uint32_t drawCount = 1,
        uint32_t stride = sizeof(VkDrawIndirectCommand)
    );

    /**
     * records the access of the buffer, then draws with its commands
     * @pre the preconditions of @ref cvk::BaseCmdBuffer::drawIndexedIndirect() and @ref use()
     */
    void drawIndexedIndirect(
        cvk::BaseBuffer const&,
        size_t offset = 0,
        uint32_t drawCount = 1,
        uint32_t stride = sizeof(VkDrawIndexedIndirectCommand)
    );

    /**
     * records the access of the buffer, then dispatches with its command
     * @pre the preconditions of @ref cvk::BaseCmdBuffer::dispatchIndirect() and @ref use()
     */
    void dispatchIndirect(cvk::BaseBuffer const&, size_t offset = 0);

    [[nodiscard]] constexpr ResourceStates const& states() const { return _states; }

protected:
    /**
     * creates the own command pool and the command buffer
     */
    BasicCmdBuffer(cvk::Core const&, cvk::queue_family_index_t);

    ~BasicCmdBuffer() { this->optDestroy(); }

    /**
     * resets the pool and the resource states
     * @pre the command buffer is not pending
     * @throws cvk::vk_result_exception result of vkResetCommandPool()
     */
    void reset();

    /**
     * appends the resource states of a later recording, records their barriers
     * @pre the preconditions of @ref cvk::Barrier::execute()
     */
    void appendStates(ResourceStates const& next);

private:
    explicit BasicCmdBuffer(std::unique_ptr<cvk::CmdPool>);

    void record(cvk::Barrier const&) const;

    ResourceStates _states{};

    // the destructor body and the base move assignment free the command buffer before the pool
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
 * @attention not thread safe
 */
class PrimaryCmdBuffer : public dev::BasicCmdBuffer<cvk::PrimaryCmdBuffer> {
public:
    /**
     * creates the own command pool and the command buffer
     * @param queueFamilyIndex of the queues it is submitted to
     * @throws cvk::vk_result_exception result of vkCreateCommandPool() or vkAllocateCommandBuffers()
     */
    PrimaryCmdBuffer(cvk::Core const&, cvk::queue_family_index_t queueFamilyIndex);

    /**
     * resets the pool and the resource states, then begins the recording
     * @param usage 0 -> any number of submits, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT -> hint: one submit
     * @pre the command buffer is not pending
     * @throws cvk::vk_result_exception result of vkResetCommandPool() or vkBeginCommandBuffer()
     */
    void begin(VkCommandBufferUsageFlags usage = 0);

    /**
     * executes the secondaries, appends their resource states with their barriers
     * @details ends the secondaries that are recording
     * @pre the preconditions of @ref cvk::PrimaryCmdBuffer::execute()
     * @pre @ref rendering() -> the secondaries need no barrier
     * @throws cvk::vk_result_exception result of vkEndCommandBuffer()
     */
    void execute(std::span<SecondaryCmdBuffer* const>);

    /**
     * executes the secondary, see @ref execute(std::span<SecondaryCmdBuffer* const>)
     */
    void execute(SecondaryCmdBuffer&);
};
}


namespace jvk {
/**
 * secondary command buffer in its own command pool, tracks the resource states of its recording, move only
 * @details @ref cvk::BaseCmdBuffer::end() is optional, @ref PrimaryCmdBuffer::execute() ends it
 * @attention not thread safe
 */
class SecondaryCmdBuffer : public dev::BasicCmdBuffer<cvk::SecondaryCmdBuffer> {
public:
    using Inheritance = cvk::SecondaryCmdBuffer::Inheritance;

    /**
     * creates the own command pool and the command buffer
     * @param queueFamilyIndex of the queues its primaries are submitted to
     * @throws cvk::vk_result_exception result of vkCreateCommandPool() or vkAllocateCommandBuffers()
     */
    SecondaryCmdBuffer(cvk::Core const&, cvk::queue_family_index_t queueFamilyIndex);

    /**
     * resets the pool and the resource states, then begins the recording outside of a rendering
     * @param usage VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT -> several pending primaries execute it
     * @pre no pending primary executes it
     * @throws cvk::vk_result_exception result of vkResetCommandPool() or vkBeginCommandBuffer()
     */
    void begin(VkCommandBufferUsageFlags usage = 0);

    /**
     * resets the pool and the resource states, then begins the recording inside the rendering of a primary
     * @pre the preconditions of @ref begin(VkCommandBufferUsageFlags)
     * @throws cvk::vk_result_exception result of vkResetCommandPool() or vkBeginCommandBuffer()
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
