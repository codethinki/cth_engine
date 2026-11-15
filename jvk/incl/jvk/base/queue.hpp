#pragma once
#include "jvk/cmd/cmd_buffer.hpp"
#include "jvk/sync/semaphore.hpp"

#include "cvk/base/queue/queue.hpp"
#include "cvk/base/queue/submit_info.hpp"
#include "cvk/render/sync/barrier.hpp"

#include <volk.h>

#include <cth/coro/executor.hpp>
#include <cth/coro/strand.hpp>
#include <cth/coro/tasks/executor_task.hpp>
#include <cth/ptr/ref_wrapper.hpp>

#include <cstdint>
#include <deque>


namespace cth::co {
class scheduler;
}

namespace jvk {
class Core;

//TODO review no jvk::Semaphore per queue that every submit signals (optional, 2026-11-09), it does not fit yet:
// submits go through vk() in the strand, the queue sees none of them, only the graph could add the signal
/**
 * serialized vulkan queue
 * @details
 * - refers to the queue of the cvk core
 * - runs its work in one strand
 * - releases the queue family ownership of resources
 * @see `release()`
 */
class Queue {
public:
    /**
     * constructs the strand on the scheduler, creates the release counter
     * @details the release command buffers and the release counter come from the core
     * @pre the cvk core and the reactor of the core are created
     * @throws cvk::vk_result_exception result of `vkCreateSemaphore()`
     */
    Queue(Core const&, cvk::Queue const&, cth::co::scheduler const&);

    /**
     * blocks until the queue finished its work
     * @attention deadlocks when called from a job of the strand
     * @attention occupies a thread of the scheduler of the strand until the queue is idle, up to several ms
     * @throws cvk::vk_result_exception result of `vkQueueWaitIdle()`
     */
    void waitIdle() const;

    //TODO review an executor_task: the receiving submit job awaits it before its own submit, so the release is
    // submitted first and the counter only changes in the strand (returning at once needs a lock around the value
    // and the post)
    //TODO review const like executor(): Queues hands out Queue const&, the release members are mutable
    //TODO review returns the jvk SemaphoreSubmitInfo: it keeps the counter for Semaphore::submitted(), it converts to
    // the cvk one
    /**
     * releases the queue family ownership of the resources in the strand
     * @details
     * - records the barriers into a release command buffer of the queue family of the queue, submits it
     * - the submit signals the next value of the release counter, `Semaphore::markSubmitted()` marks it
     * - reuses the command buffers whose value the counter reached
     * @param barriers the release halves of the queue family ownership transfers
     * @return wait for the release counter, stages `VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT`, the receiver can narrow them
     * @pre the barriers hold only queue family releases from the queue family of the queue
     * @pre the barriers come after the last use of the resources on this queue
     * @pre the receiver records the acquire halves and waits for the returned value
     * @throws cvk::vk_result_exception result of `vkQueueSubmit2()`
     * @throws cvk::vk_result_exception result of `vkCreateCommandPool()` or `vkAllocateCommandBuffers()`
     */
    [[nodiscard]] cth::co::executor_task<SemaphoreSubmitInfo> release(cvk::Barrier barriers) const;

    /**
     * @return executor of the strand
     */
    [[nodiscard]] constexpr cth::co::executor executor() const { return cth::co::executor{_strand}; }

    /**
     * @attention not serialized: use it inside the strand
     */
    [[nodiscard]] constexpr cvk::Queue const& vk() const { return *_queue; }

    /**
     * @attention not serialized: use it inside the strand
     */
    [[nodiscard]] constexpr VkQueue get() const { return _queue->get(); }

private:
    /**
     * command buffer of one release
     * @details owns its command pool, see `PrimaryCmdBuffer`
     */
    struct ReleaseCmd {
        PrimaryCmdBuffer cmd;

        /**
         * value of the release counter for the last submit of `ReleaseCmd::cmd`, free once reached
         */
        uint64_t value = 0;
    };

    /**
     * a command buffer whose last release the gpu finished, or a new one
     * @details reads the counter, no wait
     * @pre runs in the strand
     */
    [[nodiscard]] ReleaseCmd& freeReleaseCmd() const;

    cth::ref_wrapper<Core const> _core;
    cth::ref_wrapper<cvk::Queue const> _queue;
    cth::co::strand _strand;

    mutable std::deque<ReleaseCmd> _releaseCmds{}; ///< used in the strand only

    //TODO review the counter decides the reuse of the release command buffers, no reactor wait: a release reads it
    mutable Semaphore _released; ///< the release counter, raised by each release submit

public:
    Queue(Queue const& other) = delete;
    Queue(Queue&& other) = delete;
    Queue& operator=(Queue const& other) = delete;
    Queue& operator=(Queue&& other) = delete;
};
}
