#pragma once
#include "jvk/cmd/cmd_buffer.hpp"
#include "jvk/sync/semaphore.hpp"

#include "cvk/base/queue/queue.hpp"
#include "cvk/base/queue/submit_info.hpp"
#include "cvk/render/sync/barrier.hpp"

#include <volk.h>

#include <cth/coro/catching_scope.hpp>
#include <cth/coro/executor.hpp>
#include <cth/coro/strand.hpp>
#include <cth/coro/tasks/executor_task.hpp>
#include <cth/ptr/ref_wrapper.hpp>

#include <cstdint>
#include <deque>
#include <vector>


namespace cth::co {
class scheduler;
}

namespace cvk {
class Semaphore;
class Swapchain;
}

namespace jvk {
class Core;

// in queue.hpp next to Queue: it is small, no own header
/**
 * one batch of a submit on a `Queue`
 * @details
 * - `Queue::submit()` and `Queue::present()` mark the jvk signals after the submit, see
 *   `Semaphore::markSubmitted()`
 * - the binary semaphores get no mark
 */
struct SubmitInfo {
    /** the gpu executes them in order */
    std::vector<cth::ref_wrapper<PrimaryCmdBuffer const>> cmdBuffers{};

    std::vector<SemaphoreSubmitInfo> waits{};
    std::vector<cvk::SemaphoreSubmitInfo> binaryWaits{};

    std::vector<SemaphoreSubmitInfo> signals{}; ///< marked after the submit
    std::vector<cvk::SemaphoreSubmitInfo> binarySignals{};

    /**
     * removes the command buffers and the semaphores
     * @details keeps the capacity: a refill allocates nothing once the vectors are large enough
     */
    constexpr void clear() {
        cmdBuffers.clear();
        waits.clear();
        binaryWaits.clear();
        signals.clear();
        binarySignals.clear();
    }
};

/**
 * serialized vulkan queue
 * @details
 * - refers to the queue of the cvk core
 * - runs its work in one strand
 * - submits and presents, marks the jvk signals of its submits
 * - releases the queue family ownership of resources
 * @see `submit()`, `present()`, `release()`
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

    /**
     * submits the batch in the strand, then marks the jvk signals
     * @details a failed submit marks nothing: the caller submits a fallback, see
     *  `dev::fallback_submit()`
     * @pre the info lives until the end of the task, nothing changes it
     * @pre the preconditions of `cvk::Queue::submit()`
     * @throws cvk::vk_result_exception result of `vkQueueSubmit2()`
     * @see `present()`
     */
    [[nodiscard]] cth::co::executor_task<void> submit(SubmitInfo const&) const;

    /**
     * submits the batch and presents the image in one job of the strand, then marks the jvk signals
     * @details
     * - the submit signals `cvk::Swapchain::presentSemaphore()` of the image too
     * - no other job of the strand runs between the submit and the present
     * - a failed submit marks nothing and presents nothing
     * - a failed present marks the signals before it throws: the submit reached the queue
     * @param swapchain of the image
     * @param imageIdx of the acquired image
     * @return `VK_SUCCESS`, `VK_SUBOPTIMAL_KHR` or `VK_ERROR_OUT_OF_DATE_KHR`
     * @pre `cvk::Queue::canPresent()`
     * @pre the info lives until the end of the task, nothing changes it
     * @pre the preconditions of `cvk::Queue::submit()` and of `cvk::Swapchain::present()`
     * @throws cvk::vk_result_exception result of `vkQueueSubmit2()`
     * @throws cvk::vk_result_exception other result of `vkQueuePresentKHR()`
     */
    [[nodiscard]] cth::co::executor_task<VkResult> present(
        SubmitInfo const&,
        cvk::Swapchain& swapchain,
        uint32_t imageIdx
    ) const;

    // an executor_task: the receiving submit job awaits it before its own submit, so the release is
    // submitted first and the counter only changes in the strand (returning at once needs a lock around the value
    // and the post)
    // const like executor(): the core hands out Queue const&, the release members are mutable
    // returns the jvk SemaphoreSubmitInfo: it keeps the counter for Semaphore::submitted(), it
    // converts to the cvk one
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
    [[nodiscard]] constexpr cth::co::executor executor() const {
        return cth::co::executor{_strand};
    }

    /**
     * @return the scheduler of the strand, for work of the queue outside the strand
     */
    [[nodiscard]] constexpr cth::co::scheduler const& scheduler() const { return *_scheduler; }

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
     * submits the batch through `cvk::Queue::rawSubmit()`
     * @details resolves the `std::nullopt` timeline values in order: the waits, then the signals
     * @param presentSemaphorePtr `nullptr` -> no present semaphore, else signaled after the signals
     *  of the info
     * @pre runs in the strand
     * @pre the preconditions of `cvk::Queue::submit()`
     * @throws cvk::vk_result_exception result of `vkQueueSubmit2()`
     */
    void submitInStrand(
        SubmitInfo const&,
        cvk::Semaphore const* presentSemaphorePtr = nullptr
    ) const;

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
    cth::ref_wrapper<cth::co::scheduler const> _scheduler;
    cth::co::strand _strand;

    mutable std::deque<ReleaseCmd> _releaseCmds{}; ///< used in the strand only

    // members, no locals: submitInStrand() refills them, they keep their capacity
    /** the command buffers of the submit in `submitInStrand()`, used in the strand only */
    mutable std::vector<VkCommandBufferSubmitInfo> _cmdBufferInfos{};
    /** the waits of the submit in `submitInStrand()`, used in the strand only */
    mutable std::vector<VkSemaphoreSubmitInfo> _waitInfos{};
    /** the signals of the submit in `submitInStrand()`, used in the strand only */
    mutable std::vector<VkSemaphoreSubmitInfo> _signalInfos{};

    //TODO review the counter decides the reuse of the release command buffers, no reactor wait: a release reads it
    mutable Semaphore _released; ///< the release counter, raised by each release submit

public:
    Queue(Queue const& other) = delete;
    Queue(Queue&& other) noexcept = default;
    Queue& operator=(Queue const& other) = delete;
    Queue& operator=(Queue&& other) noexcept = default;
};
}

namespace jvk::dev {
/**
 * the empty submit after a failed submit: the waiters of its jvk signals go on
 * @details
 * - `Queue::submit()` marks the signals
 * - `VK_ERROR_DEVICE_LOST` -> stores the error in the scope, marks the signals: the gpu waits end
 *   on their own
 * - another failure terminates: the waiters would hang
 * @param queue of the failed submit
 * @param info waits and signals of the failed submit, no command buffers
 * @param[out] jobs of the caller, gets a device loss
 * @pre the info lives until the end of the task, nothing changes it
 */
[[nodiscard]] cth::co::executor_task<void> fallback_submit(
    Queue const& queue,
    SubmitInfo const& info,
    cth::co::catching_scope& jobs
);
}
