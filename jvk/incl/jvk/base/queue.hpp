#pragma once
#include "jvk/cmd/cmd_buffer.hpp"

#include "cvk/base/queue/queue.hpp"
#include "cvk/base/queue/submit_info.hpp"
#include "cvk/render/sync/barrier.hpp"

#include <volk.h>

#include <cth/coro/executor.hpp>
#include <cth/coro/strand.hpp>
#include <cth/coro/tasks/executor_task.hpp>
#include <cth/ptr/not_null.hpp>

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
     * constructs the strand on the scheduler
     * @details the release command buffers and the release counter come from the core
     * @pre the cvk core and the reactor of the core are created
     */
    Queue(Core const&, cvk::Queue const&, cth::co::scheduler const&);

    //TEMP the old signature until queue.cpp implements the release, Queues and the tests use it
    Queue(cvk::Queue const&, cth::co::scheduler const&);

    /**
     * blocks until the queue finished its work
     * @attention deadlocks when called from a job of the strand
     * @throws cvk::vk_result_exception result of `vkQueueWaitIdle()`
     */
    void waitIdle() const;

    //TODO review an executor_task: the receiving submit job awaits it before its own submit, so the release is
    // submitted first and the counter only changes in the strand (returning at once needs a lock around the value
    // and the post)
    //TODO review const like executor(): Queues hands out Queue const&, the release members are mutable
    /**
     * releases the queue family ownership of the resources in the strand
     * @details
     * - records the barriers into a release command buffer of the queue family of the queue, submits it
     * - the submit signals the next value of the release counter
     * @param barriers the release halves of the queue family ownership transfers
     * @return wait for the release counter, stages `VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT`, the receiver can narrow them
     * @pre the barriers hold only queue family releases from the queue family of the queue
     * @pre the barriers come after the last use of the resources on this queue
     * @pre the receiver records the acquire halves and waits for the returned value
     * @throws cvk::vk_result_exception result of `vkQueueSubmit2()`
     */
    [[nodiscard]] cth::co::executor_task<cvk::SemaphoreSubmitInfo> release(cvk::Barrier barriers) const;

    /**
     * @return executor of the strand
     */
    [[nodiscard]] constexpr cth::co::executor executor() const { return cth::co::executor{_strand}; }

    /**
     * @attention not serialized: use it inside the strand
     */
    [[nodiscard]] constexpr cvk::Queue const& vk() const { return *_queuePtr; }

    /**
     * @attention not serialized: use it inside the strand
     */
    [[nodiscard]] constexpr VkQueue get() const { return _queuePtr->get(); }

private:
    /**
     * command buffer of one release
     */
    struct ReleaseCmd {
        PrimaryCmdBuffer cmd;

        /**
         * value of the release counter for the last submit of `ReleaseCmd::cmd`, free once reached
         */
        uint64_t value = 0;
    };

    cth::not_null_ptr<cvk::Queue const> _queuePtr;
    cth::co::strand _strand;

    mutable std::deque<ReleaseCmd> _releaseCmds{}; ///< used in the strand only

    //TODO member `mutable Semaphore _released`: the release counter, raised by each release submit;
    // a member now breaks the link of queue.cpp, the destructor of jvk::Semaphore needs its source

public:
    Queue(Queue const& other) = delete;
    Queue(Queue&& other) = delete;
    Queue& operator=(Queue const& other) = delete;
    Queue& operator=(Queue&& other) = delete;
};
}
