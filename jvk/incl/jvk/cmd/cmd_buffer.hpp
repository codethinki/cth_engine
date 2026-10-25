#pragma once
#include "jvk/sync/resource_states.hpp"

#include "cvk/data/buffer/buffer.hpp"
#include "cvk/render/cmd/cmd_buffer.hpp"
#include "cvk/render/cmd/cmd_pool.hpp"
#include "cvk/render/cmd/rendering_info.hpp"

#include <volk.h>

#include <cth/ptr/not_null.hpp>

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <type_traits>
#include <vector>


namespace cvk {
class Image;
}

namespace jvk {
class PersistentSecondaryCmdBuffer;
class PrimaryCmdBuffer;
class SecondaryCmdBuffer;
}

namespace jvk::dev {
class CmdPools;

/**
 * key of the jvk command buffer constructors, only @ref CmdPools constructs it
 */
class cmd_pools_key {
    friend class CmdPools;

    constexpr explicit cmd_pools_key() = default;
};
}

namespace jvk::dev {
/**
 * cvk command buffer in its own command pool, with the resource states of its recording
 * @details base of @ref PrimaryCmdBuffer, @ref SecondaryCmdBuffer and @ref PersistentSecondaryCmdBuffer,
 *  owns the pool: one command pool per command buffer,
 *  its buffer commands record the access of the buffer, then call their cvk::BaseCmdBuffer counterpart,
 *  external Vulkan code: @ref use() the state that the code expects, the code, @ref assume() the state it leaves
 * @tparam Base cvk::PrimaryCmdBuffer or cvk::SecondaryCmdBuffer
 * @attention not thread safe: one thread at a time
 */
template<class Base>
class BasicCmdBuffer : public Base {
public:
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

protected:
    /**
     * creates the command buffer in the pool, takes the pool
     * @pre @ref cvk::CmdPool::created()
     * @throws cvk::vk_result_exception result of vkAllocateCommandBuffers()
     */
    explicit BasicCmdBuffer(std::unique_ptr<cvk::CmdPool>);

    /**
     * frees the command buffer, then destroys its pool
     * @note calls @ref cvk::BaseCmdBuffer::optDestroy()
     */
    ~BasicCmdBuffer() { this->optDestroy(); }

    /**
     * forgets the resource states of the last recording
     */
    void clearStates() { _states.clear(); }

    /**
     * appends the resource states of a later recording, records the barriers before its commands
     * @details calls @ref ResourceStates::append(), then @ref cvk::Barrier::execute()
     * @pre the preconditions of @ref cvk::Barrier::execute()
     */
    void appendStates(ResourceStates const& next);

private:
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


namespace jvk::dev {
/**
 * jvk command buffer for one submit, from a @ref CmdPools, neither copyable nor movable
 * @details base of @ref PrimaryCmdBuffer and @ref SecondaryCmdBuffer,
 *  the pools hand it out begun with VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
 *  it holds its lease until a primary or a submit takes its tasks,
 *  the last of these tasks resets its pool and gives it back to the free list of the pools,
 *  the tasks run once after the gpu finished the submit, the submit of a secondary is the submit of its primary
 * @tparam Base cvk::PrimaryCmdBuffer or cvk::SecondaryCmdBuffer
 * @attention not thread safe: one thread at a time
 */
template<class Base>
class TransientCmdBuffer : public BasicCmdBuffer<Base> {
public:
    /**
     * keeps the object until the gpu finished the submit of the command buffer, then destroys it
     * @details calls @ref add()
     */
    template<class T> requires std::constructible_from<std::decay_t<T>, T>
    void retain(T&& object);

    /**
     * adds a task that runs once after the gpu finished the submit of the command buffer
     */
    void add(std::move_only_function<void()> task);

protected:
    /**
     * creates the command buffer in the pool, takes the pool
     * @param pools to give the command buffer back to
     * @pre @ref cvk::CmdPool::created()
     * @throws cvk::vk_result_exception result of vkAllocateCommandBuffers()
     */
    TransientCmdBuffer(CmdPools& pools, std::unique_ptr<cvk::CmdPool>);

    /**
     * forgets the resource states of the last recording, the command buffer holds its lease again
     * @details @ref CmdPools calls it at the hand out
     * @pre the command buffer went back to its pools
     */
    void lease();

    /**
     * takes the tasks, appends the task that gives the command buffer back to its pools
     * @return the tasks, empty -> a primary or a submit took them since the hand out
     * @pre no thread records into the command buffer
     * @post the command buffer does not hold its lease
     */
    [[nodiscard]] std::vector<std::move_only_function<void()>> extractTasks(this auto& self);

private:
    std::vector<std::move_only_function<void()>> _tasks{};
    cth::not_null_ptr<CmdPools> _pools;

    /**
     * no primary and no submit took the tasks since the hand out
     */
    bool _leased = false;

public:
    TransientCmdBuffer(TransientCmdBuffer const& other) = delete;
    TransientCmdBuffer(TransientCmdBuffer&& other) = delete;
    TransientCmdBuffer& operator=(TransientCmdBuffer const& other) = delete;
    TransientCmdBuffer& operator=(TransientCmdBuffer&& other) = delete;
};
}

namespace jvk::dev {
extern template class TransientCmdBuffer<cvk::PrimaryCmdBuffer>;
extern template class TransientCmdBuffer<cvk::SecondaryCmdBuffer>;
}


namespace jvk {
/**
 * transient primary command buffer, neither copyable nor movable
 * @details records with the commands of @ref cvk::PrimaryCmdBuffer,
 *  @ref dev::CmdPools hands it out begun, the stage submits it,
 *  holds its pool until the gpu finished the submit, the submit runs the tasks on a scheduler worker after its fence
 * @attention not thread safe: one thread at a time
 */
class PrimaryCmdBuffer : public dev::TransientCmdBuffer<cvk::PrimaryCmdBuffer> {
public:
    /**
     * creates the command buffer in the pool, only @ref dev::CmdPools constructs it
     * @pre @ref cvk::CmdPool::created()
     * @throws cvk::vk_result_exception result of vkAllocateCommandBuffers()
     */
    PrimaryCmdBuffer(dev::cmd_pools_key, dev::CmdPools&, std::unique_ptr<cvk::CmdPool>);

    /**
     * executes the transient secondaries
     * @details ends the secondaries that did not end, takes over their tasks with their leases,
     *  appends their resource states in execute order, the barriers of a secondary before its vkCmdExecuteCommands()
     * @pre no thread records into the secondaries
     * @pre the secondaries hold their lease: no primary executed them
     * @pre the preconditions of @ref cvk::PrimaryCmdBuffer::execute()
     * @pre @ref rendering() -> the resource states of the secondaries need no barrier
     */
    void execute(std::span<SecondaryCmdBuffer* const>);

    /**
     * executes the transient secondary
     * @details calls @ref execute(std::span<SecondaryCmdBuffer* const>)
     */
    void execute(SecondaryCmdBuffer&);

    /**
     * executes the persistent secondaries
     * @details appends their resource states in execute order, the barriers of a secondary before its
     *  vkCmdExecuteCommands(), adds a task that runs the tasks of the secondaries after the gpu finished the submit,
     *  several primaries may execute one secondary at the same time, also on other threads
     * @pre the secondaries ended their recording
     * @pre a secondary without VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT: no submit of it is pending,
     *  no other primary that is not submitted executes it
     * @pre the preconditions of @ref cvk::PrimaryCmdBuffer::execute()
     * @pre @ref rendering() -> the resource states of the secondaries need no barrier
     */
    void execute(std::span<PersistentSecondaryCmdBuffer const* const>);

    /**
     * executes the persistent secondary
     * @details calls @ref execute(std::span<PersistentSecondaryCmdBuffer const* const>)
     */
    void execute(PersistentSecondaryCmdBuffer const&);

private:
    friend class dev::CmdPools;

    /**
     * begins the recording with VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
     * @details calls @ref cvk::PrimaryCmdBuffer::begin()
     * @pre !@ref recording()
     * @throws cvk::vk_result_exception result of vkBeginCommandBuffer()
     */
    void begin();

public:
    PrimaryCmdBuffer(PrimaryCmdBuffer const& other) = delete;
    PrimaryCmdBuffer(PrimaryCmdBuffer&& other) = delete;
    PrimaryCmdBuffer& operator=(PrimaryCmdBuffer const& other) = delete;
    PrimaryCmdBuffer& operator=(PrimaryCmdBuffer&& other) = delete;
};
}


namespace jvk {
/**
 * transient secondary command buffer, neither copyable nor movable
 * @details @ref dev::CmdPools hands it out begun, for use outside of a rendering or with an inheritance,
 *  the primary that executes it takes over its tasks with its lease,
 *  @ref cvk::BaseCmdBuffer::end() is optional: @ref PrimaryCmdBuffer::execute() ends a secondary that did not end,
 *  no primary executes it -> its stage takes its tasks, see @ref dev::CmdPools::extractTasks(SecondaryCmdBuffer&)
 * @attention not thread safe: one thread at a time
 */
class SecondaryCmdBuffer : public dev::TransientCmdBuffer<cvk::SecondaryCmdBuffer> {
public:
    /**
     * creates the command buffer in the pool, only @ref dev::CmdPools constructs it
     * @pre @ref cvk::CmdPool::created()
     * @throws cvk::vk_result_exception result of vkAllocateCommandBuffers()
     */
    SecondaryCmdBuffer(dev::cmd_pools_key, dev::CmdPools&, std::unique_ptr<cvk::CmdPool>);

private:
    friend class PrimaryCmdBuffer;
    friend class dev::CmdPools;

    /**
     * begins the recording for use outside of a rendering, with VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
     * @details calls @ref cvk::SecondaryCmdBuffer::begin(VkCommandBufferUsageFlags)
     * @pre !@ref recording()
     * @throws cvk::vk_result_exception result of vkBeginCommandBuffer()
     */
    void begin();

    /**
     * begins the recording for use inside the rendering of a primary, with VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
     * @details calls @ref cvk::SecondaryCmdBuffer::begin(Inheritance const&, VkCommandBufferUsageFlags)
     * @pre !@ref recording()
     * @throws cvk::vk_result_exception result of vkBeginCommandBuffer()
     */
    void begin(Inheritance const&);

public:
    SecondaryCmdBuffer(SecondaryCmdBuffer const& other) = delete;
    SecondaryCmdBuffer(SecondaryCmdBuffer&& other) = delete;
    SecondaryCmdBuffer& operator=(SecondaryCmdBuffer const& other) = delete;
    SecondaryCmdBuffer& operator=(SecondaryCmdBuffer&& other) = delete;
};
}


namespace jvk {
/**
 * secondary command buffer that primaries execute many times, owned by the caller, move only
 * @details recorded once: a stage hands it out begun, for use outside of a rendering or with an inheritance,
 *  holds its own command pool for its lifetime,
 *  the retained objects live as long as the command buffer,
 *  the tasks run after each submit that executes the command buffer,
 *  @ref PrimaryCmdBuffer::execute() requires the end of the recording
 * @attention not thread safe: one thread at a time, the primaries that execute it only read it
 */
class PersistentSecondaryCmdBuffer : public dev::BasicCmdBuffer<cvk::SecondaryCmdBuffer> {
public:
    /**
     * usage without an override: the primaries of several frames in flight execute the command buffer at the same time
     */
    static constexpr VkCommandBufferUsageFlags DEFAULT_USAGE = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

    /**
     * creates the command buffer in the pool, only @ref dev::CmdPools constructs it
     * @pre @ref cvk::CmdPool::created()
     * @throws cvk::vk_result_exception result of vkAllocateCommandBuffers()
     */
    PersistentSecondaryCmdBuffer(dev::cmd_pools_key, std::unique_ptr<cvk::CmdPool>);

    /**
     * destroys the retained objects, frees the command buffer, destroys its pool
     * @details e.g. move it into @ref PrimaryCmdBuffer::retain() of the last primary that executes it
     * @pre the gpu finished every submit that executes the command buffer, the same for a move assignment to it
     */
    ~PersistentSecondaryCmdBuffer() = default;

    /**
     * keeps the object as long as the command buffer
     */
    template<class T> requires std::constructible_from<std::decay_t<T>, T>
    void retain(T&& object);

    /**
     * adds a task that runs after the gpu finished each submit that executes the command buffer
     * @details the submits share the tasks, the tasks stay valid after the destruction of the command buffer
     * @pre @ref recording()
     * @attention runs on several workers at the same time if submits that execute the command buffer finish at the
     *  same time
     */
    void add(std::move_only_function<void() const> task);

private:
    friend class PrimaryCmdBuffer;
    friend class dev::CmdPools;

    /**
     * begins the recording for use outside of a rendering
     * @details calls @ref cvk::SecondaryCmdBuffer::begin(VkCommandBufferUsageFlags)
     * @pre !@ref recording()
     * @throws cvk::vk_result_exception result of vkBeginCommandBuffer()
     */
    void begin(VkCommandBufferUsageFlags usage);

    /**
     * begins the recording for use inside the rendering of a primary
     * @details calls @ref cvk::SecondaryCmdBuffer::begin(Inheritance const&, VkCommandBufferUsageFlags)
     * @pre !@ref recording()
     * @throws cvk::vk_result_exception result of vkBeginCommandBuffer()
     */
    void begin(Inheritance const&, VkCommandBufferUsageFlags usage);

    // never called: their destruction destroys the retained objects
    std::vector<std::move_only_function<void()>> _retained{};

    // shared with the tasks of the submits that execute the command buffer
    std::shared_ptr<std::vector<std::move_only_function<void() const>>> _tasks;

public:
    PersistentSecondaryCmdBuffer(PersistentSecondaryCmdBuffer const& other) = delete;
    PersistentSecondaryCmdBuffer(PersistentSecondaryCmdBuffer&& other) noexcept = default;
    PersistentSecondaryCmdBuffer& operator=(PersistentSecondaryCmdBuffer const& other) = delete;
    PersistentSecondaryCmdBuffer& operator=(PersistentSecondaryCmdBuffer&& other) noexcept = default;
};
}


namespace jvk {
/**
 * jvk command buffer: @ref PrimaryCmdBuffer, @ref SecondaryCmdBuffer or @ref PersistentSecondaryCmdBuffer
 */
template<class T>
concept cmd_buffer = std::derived_from<T, PrimaryCmdBuffer> || std::derived_from<T, SecondaryCmdBuffer>
    || std::derived_from<T, PersistentSecondaryCmdBuffer>;
}
