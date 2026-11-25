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
using std::span;

namespace {
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
    _transferred{core, cvk::create} { cvk::Queue::debug_check(queue); }

void Queue::waitIdle() const {
    executor().sync_wait(
        [](cvk::Queue const& queue) -> cth::co::executor_task<void> {
            queue.waitIdle();
            co_return;
        }(*_queue)
    );
}

cth::co::executor_task<void> Queue::submit(SubmitInfo const& info) {
    co_await executor().schedule();

    submitInStrand(info);
    mark(info.signals);
}

cth::co::executor_task<VkResult> Queue::present(cvk::Swapchain& swapchain, uint32_t imageIdx) {
    co_await executor().schedule();
    co_return swapchain.present(imageIdx);
}

cth::co::executor_task<SemaphoreSubmitInfo> Queue::transfer(
    cvk::Barrier barriers,
    span<SemaphoreSubmitInfo const> waits,
    span<cvk::SemaphoreSubmitInfo const> binarySignals
) {
    // all in the strand: it serializes the transfer command buffers, the counter values follow the
    // submit order
    co_await executor().schedule();

    auto& transferCmd = freeTransferCmd();
    auto& cmd = transferCmd.cmd;

    cmd.begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    barriers.execute(cmd);
    cmd.end();

    SemaphoreSubmitInfo const signal{.semaphore = _transferred, .value = _transferred.next()};
    auto& info = _transferInfo;
    info.clear();
    info.cmdBuffers.push_back(cmd);
    info.waits.append_range(waits);
    info.signals.push_back(signal);
    info.binarySignals.append_range(binarySignals);
    submitInStrand(info);

    // after the submit: a failed submit leaves the command buffer free
    transferCmd.value = signal.value;
    mark(info.signals);

    co_return signal;
}

void Queue::submitInStrand(SubmitInfo const& info) {
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

auto Queue::freeTransferCmd() -> TransferCmd& {
    auto const reached = _transferred.gpuValue();

    auto const it = std::ranges::find_if(
        _transferCmds,
        [reached](TransferCmd const& transferCmd) { return transferCmd.value <= reached; }
    );
    if(it != _transferCmds.end())
        return *it;

    return _transferCmds.emplace_back(
        TransferCmd{PrimaryCmdBuffer{*_core, _queue->familyIndex(), cvk::create}}
    );
}

namespace dev {
    cth::co::executor_task<void> skip_submit(
        Queue& queue,
        SubmitInfo const& info,
        cth::co::catching_scope& submissions
    ) {
        // every failure but a device loss terminates: nobody marks the signals, the waiters would
        // hang
        try { co_await queue.submit(info); }
        catch(cvk::vk_result_exception const& e) {
            if(e.vkResult() != VK_ERROR_DEVICE_LOST) {
                JVK_STABLE_ERR(true, "the skip submit failed, got {}", e.vkResult()) {}
                std::terminate();
            }
            submissions.store(std::current_exception());
            // the failed submit marked nothing: the waiters of the marks go on
            mark(info.signals);
        }
        catch(...) { std::terminate(); }
    }
}
}
