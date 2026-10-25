#pragma once
#include "cmd_buffer.hpp"

#include "cvk/base/queue/queue_family.hpp"
#include "cvk/render/cmd/cmd_buffer.hpp"

#include <volk.h>

#include <cth/data/hash_map.hpp>
#include <cth/ptr/not_null.hpp>

#include <deque>
#include <functional>
#include <mutex>
#include <vector>


namespace cvk {
class Core;
}

namespace jvk::dev {
/**
 * command buffers by queue family, one command pool per command buffer, neither copyable nor movable
 * @details jvk-internal: the stages hand out the command buffers, the user gets them from a stage,
 *  hands out a free transient command buffer of the family, a new one if none is free,
 *  a transient command buffer holds its lease until a primary or a submit takes its tasks,
 *  the last of these tasks resets its pool and gives it back to the free list, after the gpu finished the submit,
 *  the transient command buffers of a family grow with the number in use at the same time,
 *  a persistent secondary gets its own pool, the pools do not keep it
 * @attention thread safe
 */
class CmdPools {
public:
    /**
     * @pre @ref cvk::Core::created()
     */
    explicit CmdPools(cvk::Core const&);

    /**
     * frees the transient command buffers and destroys their pools
     * @pre every transient command buffer went back: the gpu finished its submit, its tasks ran
     */
    ~CmdPools();

    /**
     * hands out a transient primary command buffer, begun with VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
     * @param family of the queue that submits it
     * @pre the core has a queue of the family
     * @throws cvk::vk_result_exception result of vkCreateCommandPool()
     * @throws cvk::vk_result_exception result of vkAllocateCommandBuffers()
     * @throws cvk::vk_result_exception result of vkBeginCommandBuffer()
     */
    [[nodiscard]] PrimaryCmdBuffer& primary(cvk::queue_family_index_t family);

    /**
     * hands out a transient secondary command buffer for use outside of a rendering,
     *  begun with VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
     * @param family of the queue that submits the primary
     * @pre the core has a queue of the family
     * @throws cvk::vk_result_exception result of vkCreateCommandPool()
     * @throws cvk::vk_result_exception result of vkAllocateCommandBuffers()
     * @throws cvk::vk_result_exception result of vkBeginCommandBuffer()
     */
    [[nodiscard]] SecondaryCmdBuffer& secondary(cvk::queue_family_index_t family);

    /**
     * hands out a transient secondary command buffer for use inside the rendering of a primary,
     *  begun with VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
     * @param family of the queue that submits the primary
     * @param inheritance @ref cvk::PrimaryCmdBuffer::inheritance() of the rendering
     * @pre the core has a queue of the family
     * @pre @ref cvk::AttachmentMapping::debug_check() passes for the color formats of the inheritance
     * @throws cvk::vk_result_exception result of vkCreateCommandPool()
     * @throws cvk::vk_result_exception result of vkAllocateCommandBuffers()
     * @throws cvk::vk_result_exception result of vkBeginCommandBuffer()
     */
    [[nodiscard]] SecondaryCmdBuffer& secondary(
        cvk::queue_family_index_t family,
        cvk::SecondaryCmdBuffer::Inheritance const& inheritance
    );

    /**
     * creates a persistent secondary command buffer in its own pool, begun for use outside of a rendering
     * @param family of the queues that submit the primaries
     * @param usage without VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT -> one primary at a time executes it
     * @pre the core has a queue of the family
     * @throws cvk::vk_result_exception result of vkCreateCommandPool()
     * @throws cvk::vk_result_exception result of vkAllocateCommandBuffers()
     * @throws cvk::vk_result_exception result of vkBeginCommandBuffer()
     */
    [[nodiscard]] PersistentSecondaryCmdBuffer persistentSecondary(
        cvk::queue_family_index_t family,
        VkCommandBufferUsageFlags usage = PersistentSecondaryCmdBuffer::DEFAULT_USAGE
    ) const;

    /**
     * creates a persistent secondary command buffer in its own pool, begun for use inside the renderings of primaries
     * @param family of the queues that submit the primaries
     * @param inheritance @ref cvk::PrimaryCmdBuffer::inheritance() of the renderings
     * @param usage without VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT -> one primary at a time executes it
     * @pre the core has a queue of the family
     * @pre @ref cvk::AttachmentMapping::debug_check() passes for the color formats of the inheritance
     * @throws cvk::vk_result_exception result of vkCreateCommandPool()
     * @throws cvk::vk_result_exception result of vkAllocateCommandBuffers()
     * @throws cvk::vk_result_exception result of vkBeginCommandBuffer()
     */
    [[nodiscard]] PersistentSecondaryCmdBuffer persistentSecondary(
        cvk::queue_family_index_t family,
        cvk::SecondaryCmdBuffer::Inheritance const& inheritance,
        VkCommandBufferUsageFlags usage = PersistentSecondaryCmdBuffer::DEFAULT_USAGE
    ) const;

    /**
     * takes the tasks of the transient primary, the last task gives it back
     * @details the submit calls it after the end of the primary and runs the tasks after the gpu finished the submit,
     *  a skip runs them at once
     * @return the tasks, empty -> they were taken since the hand out
     */
    [[nodiscard]] static std::vector<std::move_only_function<void()>> extractTasks(PrimaryCmdBuffer&);

    /**
     * takes the tasks of the transient secondary, the last task gives it back
     * @details for a secondary that no primary executed: the stage calls it at its submit or skip of the frame,
     *  for every secondary it handed out in the frame,
     *  the tasks run after the gpu finished the submit, at once at a skip
     * @return the tasks, empty -> the primary that executes it took them
     * @pre no thread records into the secondary
     */
    [[nodiscard]] static std::vector<std::move_only_function<void()>> extractTasks(SecondaryCmdBuffer&);

    [[nodiscard]] constexpr cvk::Core const& core() const { return *_core; }

private:
    template<class Base>
    friend class TransientCmdBuffer;

    /**
     * resets the pool of the command buffer, adds the command buffer to the free list of its family
     * @details the task of @ref TransientCmdBuffer::extractTasks() calls it
     * @pre the gpu finished every submit of the command buffer
     * @throws cvk::vk_result_exception result of vkResetCommandPool()
     */
    void giveBack(PrimaryCmdBuffer&);

    /**
     * resets the pool of the command buffer, adds the command buffer to the free list of its family
     * @details the task of @ref TransientCmdBuffer::extractTasks() calls it
     * @pre the gpu finished every submit of the command buffer
     * @throws cvk::vk_result_exception result of vkResetCommandPool()
     */
    void giveBack(SecondaryCmdBuffer&);

    /**
     * transient command buffers of one queue family
     */
    struct family_buffers {
        // the command buffers do not move: a deque keeps their addresses
        std::deque<PrimaryCmdBuffer> primaries{};
        std::deque<SecondaryCmdBuffer> secondaries{};

        std::vector<cth::not_null_ptr<PrimaryCmdBuffer>> freePrimaries{};
        std::vector<cth::not_null_ptr<SecondaryCmdBuffer>> freeSecondaries{};
    };

    cth::not_null_ptr<cvk::Core const> _core;
    std::mutex _mutex{};

    // a rehash moves the deques, not their command buffers
    cth::dt::flat_hash_map<cvk::queue_family_index_t, family_buffers> _families{};

public:
    CmdPools(CmdPools const& other) = delete;
    CmdPools(CmdPools&& other) = delete;
    CmdPools& operator=(CmdPools const& other) = delete;
    CmdPools& operator=(CmdPools&& other) = delete;
};
}
