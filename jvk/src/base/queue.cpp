#include "jvk/base/queue.hpp"

#include "jvk/base/core.hpp"

#include <cth/coro/scheduler.hpp>
#include <cth/coro/tasks/executor_task.hpp>

#include <algorithm>


namespace jvk {
Queue::Queue(Core const& core, cvk::Queue const& queue, cth::co::scheduler const& queueScheduler) :
    _core{core}, _queue{queue}, _scheduler{queueScheduler}, _strand{queueScheduler}, _released{core, cvk::create} {
    cvk::Queue::debug_check(queue);
}

void Queue::waitIdle() const {
    executor().sync_wait(
        [](cvk::Queue const& queue) -> cth::co::executor_task<void> {
            queue.waitIdle();
            co_return;
        }(*_queue)
    );
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
    _queue->submit({.cmdBuffers = {cmd}, .signals = {signal}});

    // after the submit: a failed submit leaves the command buffer free
    releaseCmd.value = signal.value;
    _released.markSubmitted(signal.value);

    co_return signal;
}

auto Queue::freeReleaseCmd() const -> ReleaseCmd& {
    auto const reached = _released.gpuValue();

    auto const it = std::ranges::find_if(_releaseCmds, [reached](ReleaseCmd const& releaseCmd) {
        return releaseCmd.value <= reached;
    });
    if(it != _releaseCmds.end()) return *it;

    return _releaseCmds.emplace_back(ReleaseCmd{PrimaryCmdBuffer{*_core, _queue->familyIndex()}});
}
}
