#include "jvk/base/queue.hpp"

#include <cth/coro/scheduler.hpp>
#include <cth/coro/tasks/executor_task.hpp>


namespace jvk {
Queue::Queue(cvk::Queue const& queue, cth::co::scheduler const& queueScheduler) :
    _queuePtr{&queue}, _strand{queueScheduler} {
    cvk::Queue::debug_check(queue);
}

void Queue::waitIdle() const {
    executor().sync_wait(
        [](cvk::Queue const& queue) -> cth::co::executor_task<void> {
            queue.waitIdle();
            co_return;
        }(*_queuePtr)
    );
}
}
