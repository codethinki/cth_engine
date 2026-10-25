#include "jvk/base/queue.hpp"

#include "context.hpp"
#include "test.hpp"

#include "cvk/render/sync/timeline_semaphore.hpp"

#include <cth/coro/tasks/executor_task.hpp>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <thread>


namespace jvk {
namespace {
    constexpr size_t ROUNDS = 50;

    /**
     * jobs inside the strand
     */
    struct StrandJobs {
        std::atomic<size_t> running{0};
        std::atomic<size_t> maxRunning{0};
        std::atomic<size_t> outsideStrand{0};
    };

    /**
     * enters the strand and leaves it to the workers, each round
     */
    cth::co::executor_task<void> enter_rounds(Core const& core, Queue const& queue, StrandJobs& jobs) {
        for(size_t round = 0; round < ROUNDS; ++round) {
            co_await queue.executor().schedule();

            if(!queue.executor().context().owns_thread()) ++jobs.outsideStrand;
            auto const now = jobs.running.fetch_add(1) + 1;
            auto max = jobs.maxRunning.load();
            while(max < now && !jobs.maxRunning.compare_exchange_weak(max, now)) {}

            std::this_thread::sleep_for(std::chrono::microseconds{20}); // widens the window for an overlap
            jobs.running.fetch_sub(1);

            co_await core.executor().schedule();
        }
    }

    /**
     * submits inside the strand: signals the semaphore
     */
    cth::co::executor_task<void> signal(Queue const& queue, cvk::TimelineSemaphore const& semaphore) {
        co_await queue.executor().schedule();
        queue.vk().submit({.signals = {cvk::SemaphoreSubmitInfo{semaphore}}});
    }
}

JVK_TEST(Queue, jobs_of_two_coroutines_never_overlap) {
    test::Context const context{};
    auto const& queue = context.core.queue(0);
    StrandJobs jobs{};

    std::jthread other{[&] { context.core.executor().sync_wait(enter_rounds(context.core, queue, jobs)); }};
    context.core.executor().sync_wait(enter_rounds(context.core, queue, jobs));
    other.join();

    EXPECT_EQ(jobs.outsideStrand.load(), 0);
    EXPECT_EQ(jobs.maxRunning.load(), 1);
}

JVK_TEST(Queue, wait_idle_waits_for_the_submits) {
    test::Context const context{};
    auto const& queue = context.core.queue(0);
    cvk::TimelineSemaphore const semaphore{context.core, cvk::create};

    queue.executor().sync_wait(signal(queue, semaphore));
    queue.waitIdle();

    EXPECT_EQ(semaphore.gpuValue(), 1);
}
}
