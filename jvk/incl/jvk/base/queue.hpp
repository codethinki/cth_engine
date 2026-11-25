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
#include <span>
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
 * - `Queue::submit()` and `Queue::transfer()` mark the jvk signals after the submit, see
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
 * - transfers the queue family ownership of resources
 * @see `submit()`, `present()`, `transfer()`
 */
class Queue {
public:
    /**
     * constructs the strand on the scheduler, creates the transfer counter
     * @details the transfer command buffers and the transfer counter come from the core
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
     * @details a failed submit marks nothing: the caller skips it, see `dev::skip_submit()`
     * @pre the info lives until the end of the task, nothing changes it
     * @pre the preconditions of `cvk::Queue::submit()`
     * @throws cvk::vk_result_exception result of `vkQueueSubmit2()`
     */
    [[nodiscard]] cth::co::executor_task<void> submit(SubmitInfo const&);

    /**
     * presents the image in one job of the strand
     * @details one `vkQueuePresentKHR()` with the present fence of the image, see
     *  `cvk::Swapchain::present()`
     * @param swapchain of the image, presents through this queue
     * @param imageIdx of the acquired image, a submit before signals its present semaphore
     * @return `VK_SUCCESS`, `VK_SUBOPTIMAL_KHR` or `VK_ERROR_OUT_OF_DATE_KHR`
     * @pre `cvk::Queue::canPresent()`
     * @pre the swapchain lives until the end of the task
     * @pre the preconditions of `cvk::Swapchain::present()`
     * @throws cvk::vk_result_exception other result of `vkQueuePresentKHR()`
     */
    [[nodiscard]] cth::co::executor_task<VkResult> present(
        cvk::Swapchain& swapchain,
        uint32_t imageIdx
    );

    // an executor_task: the receiving submission awaits it before its own submit, so the transfer
    // is submitted first and the counter only changes in the strand (returning at once needs a lock
    // around the value and the post)
    // returns the jvk SemaphoreSubmitInfo: it keeps the counter for Semaphore::submitted(), it
    // converts to the cvk one
    /**
     * transfers the queue family ownership of resources in the strand: release or acquire halves
     * @details
     * - records the barriers into a transfer command buffer of the family of the queue, submits it
     * - the submit waits for the waits, signals the next value of the transfer counter and the
     *   binary signals, `Semaphore::markSubmitted()` marks the counter
     * - reuses the command buffers whose value the counter reached
     * @param barriers the release or the acquire halves of the ownership transfers, for the family
     *  of the queue
     * @param waits of the submit, e.g. the submit of the release halves before acquire halves
     * @param binarySignals of the submit, e.g. the present semaphores of the transferred images
     * @return wait for the transfer counter, stages `VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT`, the
     *  receiver can narrow them
     * @pre release halves: after the last use of the resources on this queue, the receiver records
     *  the acquire halves and waits for the returned value
     * @pre acquire halves: the waits cover the submit of their release halves
     * @pre the waits and the binary signals live until the end of the task, unchanged
     * @throws cvk::vk_result_exception result of `vkQueueSubmit2()`
     * @throws cvk::vk_result_exception result of `vkCreateCommandPool()` or `vkAllocateCommandBuffers()`
     */
    [[nodiscard]] cth::co::executor_task<SemaphoreSubmitInfo> transfer(
        cvk::Barrier barriers,
        std::span<SemaphoreSubmitInfo const> waits = {},
        std::span<cvk::SemaphoreSubmitInfo const> binarySignals = {}
    );

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
    void submitInStrand(SubmitInfo const&);

    struct TransferCmd {
        PrimaryCmdBuffer cmd;

        uint64_t value = 0; ///< free once the transfer counter reached it
    };

    [[nodiscard]] TransferCmd& freeTransferCmd();

    cth::ref_wrapper<Core const> _core;
    cth::ref_wrapper<cvk::Queue const> _queue;
    cth::ref_wrapper<cth::co::scheduler const> _scheduler;
    cth::co::strand _strand;

    // used in the strand only, members and no locals: they keep their capacity
    std::deque<TransferCmd> _transferCmds{};
    SubmitInfo _transferInfo{};

    std::vector<VkCommandBufferSubmitInfo> _cmdBufferInfos{};
    std::vector<VkSemaphoreSubmitInfo> _waitInfos{};
    std::vector<VkSemaphoreSubmitInfo> _signalInfos{};

    /**
     * the counter decides the reuse of the transfer command buffers, no reactor wait: a transfer
     * reads it
     */
    Semaphore _transferred;

public:
    Queue(Queue const& other) = delete;
    Queue(Queue&& other) noexcept = default;
    Queue& operator=(Queue const& other) = delete;
    Queue& operator=(Queue&& other) noexcept = default;
};
}

namespace jvk::dev {
/** terminates on a failure other than a device loss: the waiters of the signals would hang */
[[nodiscard]] cth::co::executor_task<void> skip_submit(
    Queue& queue,
    SubmitInfo const& info,
    cth::co::catching_scope& submissions
);
}
