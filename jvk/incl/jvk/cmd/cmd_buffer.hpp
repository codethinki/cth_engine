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
#include <type_traits>


namespace cvk {
class Core;
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
inline constexpr bool access_pairs<Resource, Access, Rest...> = access_pair<Resource, Access> && access_pairs<Rest...>;
}

namespace jvk {
/**
 * alternating resources and their accesses, at least two pairs
 * @details pairs: (cvk::Image or derived, @ref ImageAccess) or (cvk::BaseBuffer or derived, @ref BufferAccess)
 */
template<class... Args>
concept access_pairs = sizeof...(Args) >= 4 && dev::access_pairs<std::remove_cvref_t<Args>...>;
}

namespace jvk::dev {
/**
 * base of the jvk command buffers
 * @details owns a command pool
 * @details tracks resource states
 * @details external Vulkan code: @ref use() before, @ref assume() after
 * @tparam Base cvk::PrimaryCmdBuffer or cvk::SecondaryCmdBuffer
 * @attention not thread safe
 */
template<class Base>
class BasicCmdBuffer : public Base {
public:
    static constexpr VkCommandPoolCreateFlags POOL_FLAGS = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;

    /**
     * tracks an access of the image that no tracked command records
     * @details records the barrier from the tracked state
     * @pre @ref cvk::BaseCmdBuffer::recording()
     * @pre @ref cvk::BaseCmdBuffer::rendering() -> only a local read barrier, see @ref cvk::Barrier::execute()
     */
    void use(cvk::Image const&, ImageAccess const&);

    /**
     * tracks an access of the buffer that no tracked command records
     * @details records the barrier from the tracked state
     * @pre @ref cvk::BaseCmdBuffer::recording()
     * @pre @ref cvk::BaseCmdBuffer::rendering() -> only a local read barrier, see @ref cvk::Barrier::execute()
     */
    void use(cvk::BaseBuffer const&, BufferAccess const&);

    /**
     * tracks the accesses of several resources
     * @details records one barrier for all accesses, in argument order
     * @param args pairs of a resource and its access, see @ref access_pairs
     * @pre the preconditions of the single @ref use() per pair
     */
    template<class... Args> requires access_pairs<Args...>
    void use(Args const&... args) {
        cvk::Barrier barrier{};
        addAccesses(barrier, args...);
        record(barrier);
    }

    /**
     * sets the tracked state of the mip levels after untracked code, see @ref ResourceStates::assume()
     * @pre @ref cvk::BaseCmdBuffer::recording()
     */
    void assume(
        cvk::Image const&,
        ImageState const& state,
        uint32_t firstMipLevel = 0,
        uint32_t mipLevels = VK_REMAINING_MIP_LEVELS
    );

    /**
     * sets the tracked state of the buffer after untracked code, see @ref ResourceStates::assume()
     * @pre @ref cvk::BaseCmdBuffer::recording()
     */
    void assume(cvk::BaseBuffer const&, BufferState const& state);

    /**
     * begins the dynamic rendering
     * @details tracks the accesses of the attachments
     * @details discard: VK_ATTACHMENT_LOAD_OP_CLEAR or VK_ATTACHMENT_LOAD_OP_DONT_CARE -> the whole mip level
     *  loses its contents, also outside the view and the render area
     * @pre the preconditions of @ref cvk::BaseCmdBuffer::beginRendering()
     * @pre the depth and the stencil attachment of one image have the same layout
     */
    void beginRendering(cvk::RenderingInfo const&);

    /**
     * ends the dynamic rendering
     * @details adds the attachment accesses to the last states of the attachments
     * @pre the preconditions of @ref cvk::BaseCmdBuffer::endRendering()
     */
    void endRendering();

    /**
     * binds the buffer to the vertex input binding
     * @details tracks the read of the buffer at the bind: write the buffer before the bind
     * @pre the preconditions of @ref cvk::BaseCmdBuffer::bindVertexBuffer() and @ref use()
     */
    void bindVertexBuffer(uint32_t binding, cvk::BaseBuffer const&, size_t offset = 0);

    /**
     * binds the index buffer
     * @details tracks the read of the buffer at the bind: write the buffer before the bind
     * @pre the preconditions of @ref cvk::BaseCmdBuffer::bindIndexBuffer() and @ref use()
     */
    void bindIndexBuffer(cvk::BaseBuffer const&, VkIndexType, size_t offset = 0);

    /**
     * binds the index buffer with the index type of T
     */
    template<cvk::index_element T>
    void bindIndexBuffer(cvk::Buffer<T> const& buffer, size_t offset = 0) {
        bindIndexBuffer(static_cast<cvk::BaseBuffer const&>(buffer), cvk::index_type<T>, offset);
    }

    /**
     * draws with the VkDrawIndirectCommands in the buffer
     * @details tracks the read of the buffer
     * @pre the preconditions of @ref cvk::BaseCmdBuffer::drawIndirect() and @ref use()
     */
    void drawIndirect(
        cvk::BaseBuffer const&,
        size_t offset = 0,
        uint32_t drawCount = 1,
        uint32_t stride = sizeof(VkDrawIndirectCommand)
    );

    /**
     * draws with the VkDrawIndexedIndirectCommands in the buffer
     * @details tracks the read of the buffer
     * @pre the preconditions of @ref cvk::BaseCmdBuffer::drawIndexedIndirect() and @ref use()
     */
    void drawIndexedIndirect(
        cvk::BaseBuffer const&,
        size_t offset = 0,
        uint32_t drawCount = 1,
        uint32_t stride = sizeof(VkDrawIndexedIndirectCommand)
    );

    /**
     * dispatches with the VkDispatchIndirectCommand in the buffer
     * @details tracks the read of the buffer
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
     * appends the resource states of a later recording
     * @details records the barriers for its first accesses
     * @pre the preconditions of @ref cvk::Barrier::execute()
     */
    void appendStates(ResourceStates const& next);

private:
    explicit BasicCmdBuffer(std::unique_ptr<cvk::CmdPool>);

    /**
     * records the barrier if it is not empty
     * @details inside a rendering: adds VK_DEPENDENCY_BY_REGION_BIT, with multiview VK_DEPENDENCY_VIEW_LOCAL_BIT
     */
    void record(cvk::Barrier&) const;

    template<class Resource, class Access, class... Rest>
    void addAccesses(cvk::Barrier& barrier, Resource const& resource, Access const& access, Rest const&... rest) {
        _states.access(resource, access, barrier);
        if constexpr(sizeof...(Rest) > 0) addAccesses(barrier, rest...);
    }

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
 * primary command buffer
 * @details owns a command pool
 * @details tracks resource states
 * @attention not thread safe
 * @attention do not move into a cvk command buffer value: the pool stays behind
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
     * begins the recording
     * @details resets the pool and the resource states
     * @param usage 0 -> any number of submits, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT -> hint: one submit
     * @pre the command buffer is not pending
     * @throws cvk::vk_result_exception result of vkResetCommandPool() or vkBeginCommandBuffer()
     */
    void begin(VkCommandBufferUsageFlags usage = 0);

    /**
     * executes the secondaries
     * @details calls @ref optEnd() on the secondaries
     * @details appends the resource states of the secondaries
     * @pre the preconditions of @ref cvk::PrimaryCmdBuffer::execute()
     * @pre @ref rendering() -> the secondaries need only local read barriers, see @ref cvk::Barrier::execute()
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
 * secondary command buffer
 * @details owns a command pool
 * @details tracks resource states
 * @details @ref cvk::BaseCmdBuffer::end() is optional, @ref PrimaryCmdBuffer::execute() ends it
 * @attention not thread safe
 * @attention do not move into a cvk command buffer value: the pool stays behind
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
     * begins the recording outside of a rendering
     * @details resets the pool and the resource states
     * @param usage VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT -> several pending primaries execute it
     * @pre no pending primary executes it
     * @throws cvk::vk_result_exception result of vkResetCommandPool() or vkBeginCommandBuffer()
     */
    void begin(VkCommandBufferUsageFlags usage = 0);

    /**
     * begins the recording inside the rendering of a primary
     * @details resets the pool and the resource states
     * @pre the preconditions of @ref begin(VkCommandBufferUsageFlags)
     * @throws cvk::vk_result_exception result of vkResetCommandPool() or vkBeginCommandBuffer()
     */
    void begin(Inheritance const&, VkCommandBufferUsageFlags usage = 0);
};
}


namespace jvk {
/**
 * jvk command buffer
 * @details @ref PrimaryCmdBuffer or @ref SecondaryCmdBuffer
 */
template<class T>
concept cmd_buffer = std::derived_from<T, PrimaryCmdBuffer> || std::derived_from<T, SecondaryCmdBuffer>;
}
