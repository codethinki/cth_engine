#include "jvk/base/queue.hpp"

#include "jvk/base/core.hpp"

#include "cvk/surface/swapchain.hpp"
#include "cvk/utility/vk_exceptions.hpp"

#include <cth/coro/scheduler.hpp>
#include <cth/coro/tasks/executor_task.hpp>

#include <algorithm>
#include <exception>
#include <span>
#include <utility>


namespace jvk {
namespace {
    /**
     * the cvk batch: the jvk semaphores, then the binary ones
     */
    [[nodiscard]] cvk::SubmitInfo to_cvk(SubmitInfo const& info) {
        cvk::SubmitInfo vkInfo{};
        vkInfo.cmdBuffers.reserve(info.cmdBuffers.size());
        for(PrimaryCmdBuffer const& cmdBuffer : info.cmdBuffers)
            vkInfo.cmdBuffers.emplace_back(cmdBuffer);

        vkInfo.waits.reserve(info.waits.size() + info.binaryWaits.size());
        vkInfo.waits.append_range(info.waits);
        vkInfo.waits.append_range(info.binaryWaits);

        vkInfo.signals.reserve(info.signals.size() + info.binarySignals.size());
        vkInfo.signals.append_range(info.signals);
        vkInfo.signals.append_range(info.binarySignals);
        return vkInfo;
    }

    /**
     * marks the jvk signals, see `Semaphore::markSubmitted()`
     */
    void mark(std::span<SemaphoreSubmitInfo const> signals) {
        for(auto const& signal : signals)
            signal.semaphore->markSubmitted(signal.value);
    }
}

Queue::Queue(Core const& core, cvk::Queue const& queue, cth::co::scheduler const& queueScheduler) :
    _core{core},
    _queue{queue},
    _scheduler{queueScheduler},
    _strand{queueScheduler},
    _released{core, cvk::create} { cvk::Queue::debug_check(queue); }

void Queue::waitIdle() const {
    executor().sync_wait(
        [](cvk::Queue const& queue) -> cth::co::executor_task<void> {
            queue.waitIdle();
            co_return;
        }(*_queue)
    );
}

cth::co::executor_task<void> Queue::submit(SubmitInfo info) const {
    co_await executor().schedule();

    submitInStrand(info);
    mark(info.signals);
}

cth::co::executor_task<VkResult> Queue::present(
    SubmitInfo info,
    cvk::Swapchain& swapchain,
    uint32_t imageIdx
) const {
    info.binarySignals.emplace_back(swapchain.presentSemaphore(imageIdx));

    // no suspension between the submit and the present: the presents follow the order of the
    // submits
    co_await executor().schedule();
    submitInStrand(info);

    // the marks after the present: a waiter for them uses the swapchain after vkQueuePresentKHR()
    VkResult result{};
    try { result = swapchain.present(imageIdx); }
    catch(cvk::vk_result_exception const&) {
        // the submit reached the queue: the gpu signals the values
        mark(info.signals);
        throw;
    }
    mark(info.signals);

    co_return result;
}

cth::co::executor_task<SemaphoreSubmitInfo> Queue::release(cvk::Barrier barriers) const {
    // all in the strand: it serializes the release command buffers, the counter values follow the submit order
    co_await executor().schedule();

    auto& releaseCmd = freeReleaseCmd();
    auto& cmd = releaseCmd.cmd;

    cmd.begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    barriers.execute(cmd);
    cmd.end();

    SemaphoreSubmitInfo const signal{.semaphore = _released, .value = _released.next()};
    SubmitInfo const info{.cmdBuffers = {cmd}, .signals = {signal}};
    submitInStrand(info);

    // after the submit: a failed submit leaves the command buffer free
    releaseCmd.value = signal.value;
    mark(info.signals);

    co_return signal;
}

void Queue::submitInStrand(SubmitInfo const& info) const { _queue->submit(to_cvk(info)); }

auto Queue::freeReleaseCmd() const -> ReleaseCmd& {
    auto const reached = _released.gpuValue();

    auto const it = std::ranges::find_if(
        _releaseCmds,
        [reached](ReleaseCmd const& releaseCmd) { return releaseCmd.value <= reached; }
    );
    if(it != _releaseCmds.end())
        return *it;

    return _releaseCmds.emplace_back(
        ReleaseCmd{PrimaryCmdBuffer{*_core, _queue->familyIndex(), cvk::create}}
    );
}

namespace dev {
    cth::co::executor_task<void> fallback_submit(
        Queue const& queue,
        SubmitInfo info,
        first_error& error
    ) {
        // a copy: the submit takes the info
        auto const signals = info.signals;

        try { co_await queue.submit(std::move(info)); }
        catch(cvk::vk_result_exception const& e) {
            if(e.vkResult() != VK_ERROR_DEVICE_LOST)
                throw;
            error.store(std::current_exception());
            // the failed submit marked nothing: the waiters of the marks go on
            mark(signals);
        }
    }
}
}
