#include "jvk/base/queue.hpp"

#include "jvk/base/core.hpp"
#include "jvk/utility/exception.hpp"

#include "cvk/surface/swapchain.hpp"
#include "cvk/utility/vk_exceptions.hpp"

#include <cth/coro/scheduler.hpp>
#include <cth/coro/tasks/executor_task.hpp>

#include <algorithm>
#include <exception>
#include <span>


namespace jvk {
namespace {
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

cth::co::executor_task<void> Queue::submit(SubmitInfo const& info) const {
    co_await executor().schedule();

    submitInStrand(info);
    mark(info.signals);
}

cth::co::executor_task<VkResult> Queue::present(
    SubmitInfo const& info,
    cvk::Swapchain& swapchain,
    uint32_t imageIdx
) const {
    // no suspension between the submit and the present: the presents follow the order of the
    // submits
    co_await executor().schedule();
    submitInStrand(info, &swapchain.presentSemaphore(imageIdx));

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

void Queue::submitInStrand(
    SubmitInfo const& info,
    cvk::Semaphore const* presentSemaphorePtr
) const {
    _cmdBufferInfos.clear();
    for(PrimaryCmdBuffer const& cmdBuffer : info.cmdBuffers)
        _cmdBufferInfos.push_back(cvk::cmd_buffer_submit_info(cmdBuffer));

    // the waits first: the order of the value resolution in cvk::Queue::submit()
    _waitInfos.clear();
    for(auto const& wait : info.waits)
        _waitInfos.push_back(cvk::wait_submit_info(wait));
    for(auto const& wait : info.binaryWaits)
        _waitInfos.push_back(cvk::wait_submit_info(wait));

    _signalInfos.clear();
    for(auto const& signal : info.signals)
        _signalInfos.push_back(cvk::signal_submit_info(signal));
    for(auto const& signal : info.binarySignals)
        _signalInfos.push_back(cvk::signal_submit_info(signal));
    if(presentSemaphorePtr != nullptr)
        _signalInfos.push_back(cvk::signal_submit_info({*presentSemaphorePtr}));

    _queue->rawSubmit(
        VkSubmitInfo2{
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
            .pNext = nullptr,
            .flags = 0,
            .waitSemaphoreInfoCount = static_cast<uint32_t>(_waitInfos.size()),
            .pWaitSemaphoreInfos = _waitInfos.data(),
            .commandBufferInfoCount = static_cast<uint32_t>(_cmdBufferInfos.size()),
            .pCommandBufferInfos = _cmdBufferInfos.data(),
            .signalSemaphoreInfoCount = static_cast<uint32_t>(_signalInfos.size()),
            .pSignalSemaphoreInfos = _signalInfos.data(),
        }
    );
}

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
    cth::co::executor_task<void> skip_submit(
        Queue const& queue,
        SubmitInfo const& info,
        cth::co::catching_scope& jobs
    ) {
        // every failure but a device loss terminates: nobody marks the signals, the waiters would
        // hang
        try { co_await queue.submit(info); }
        catch(cvk::vk_result_exception const& e) {
            if(e.vkResult() != VK_ERROR_DEVICE_LOST) {
                JVK_STABLE_ERR(true, "the skip submit failed, got {}", e.vkResult()) {}
                std::terminate();
            }
            jobs.store(std::current_exception());
            // the failed submit marked nothing: the waiters of the marks go on
            mark(info.signals);
        }
        catch(...) { std::terminate(); }
    }
}
}
