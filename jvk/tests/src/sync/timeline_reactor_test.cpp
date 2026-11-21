#include "jvk/sync/timeline_reactor.hpp"

#include "context.hpp"
#include "test.hpp"

#include "jvk/base/queue.hpp"

#include "cvk/render/sync/timeline_semaphore.hpp"

#include <cth/coro/tasks/executor_task.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <future>
#include <system_error>
#include <vector>


namespace jvk {
namespace {
    using namespace std::chrono_literals;

    constexpr auto REACHED_TIMEOUT = 5s; ///< a reached value calls its callback before it
    constexpr auto PENDING_TIMEOUT = 50ms; ///< a pending callback does not come within it

    /**
     * what the callback of a wait got
     */
    struct CallbackResult {
        std::error_code error;
        bool onWorker;
    };

    /**
     * registers a callback with the reactor of the core
     * @return result of the callback
     */
    [[nodiscard]] std::future<CallbackResult> await_value(
        Core const& core,
        VkSemaphore vkSemaphore,
        uint64_t value
    ) {
        std::promise<CallbackResult> promise{};
        auto future = promise.get_future();
        core.reactor().await(
            vkSemaphore,
            value,
            [&core, promise = std::move(promise)](std::error_code error) mutable {
                promise.set_value({error, core.executor().context().owns_thread()});
            }
        );
        return future;
    }

    [[nodiscard]] bool reached(std::future<CallbackResult> const& future) {
        return future.wait_for(REACHED_TIMEOUT) == std::future_status::ready;
    }

    [[nodiscard]] bool pending(std::future<CallbackResult> const& future) {
        return future.wait_for(PENDING_TIMEOUT) == std::future_status::timeout;
    }

    /**
     * signals the value on the gpu, submits inside the strand of the queue
     */
    cth::co::executor_task<void> gpu_signal(
        Queue const& queue,
        cvk::TimelineSemaphore const& semaphore,
        uint64_t value
    ) {
        co_await queue.executor().schedule();
        queue.vk().submit({.signals = {{semaphore, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, value}}});
    }

    void gpu_signal(Core const& core, cvk::TimelineSemaphore const& semaphore, uint64_t value) {
        core.executor().sync_wait(gpu_signal(core.queue(0), semaphore, value));
    }

    /**
     * signals from the host until the semaphore has the value
     */
    void host_signal(cvk::TimelineSemaphore& semaphore, uint64_t value) {
        while(semaphore.value() < value)
            [[maybe_unused]] auto const signaled = semaphore.signal();
    }
}

JVK_TEST(TimelineReactor, base_constructor_does_not_create) {
    test::Context const context{};
    cth::co::scheduler const workers{};

    TimelineReactor const reactor{context.core, workers};

    EXPECT_FALSE(reactor.created());
}

JVK_TEST(TimelineReactor, create_and_destroy) {
    test::Context const context{};
    cth::co::scheduler workers{cth::co::autostart};
    TimelineReactor reactor{context.core, workers};

    reactor.create();
    EXPECT_TRUE(reactor.created());

    reactor.destroy();
    EXPECT_FALSE(reactor.created());

    reactor.create();
    EXPECT_TRUE(reactor.created());
}

JVK_TEST(TimelineReactor, gpu_signal_calls_the_callback_on_a_worker) {
    test::Context const context{};
    cvk::TimelineSemaphore const semaphore{context.core, cvk::create};

    auto result = await_value(context.core, semaphore.get(), 1);
    EXPECT_TRUE(context.core.reactor().pending(semaphore.get()));

    gpu_signal(context.core, semaphore, 1);

    ASSERT_TRUE(reached(result));
    auto const [error, onWorker] = result.get();
    EXPECT_FALSE(error);
    EXPECT_TRUE(onWorker);
    EXPECT_FALSE(context.core.reactor().pending(semaphore.get()));
}

JVK_TEST(TimelineReactor, several_semaphores_and_values_out_of_order) {
    test::Context const context{};
    cvk::TimelineSemaphore const first{context.core, cvk::create};
    cvk::TimelineSemaphore const second{context.core, cvk::create};

    auto first3 = await_value(context.core, first.get(), 3);
    auto second1 = await_value(context.core, second.get(), 1);
    auto first1 = await_value(context.core, first.get(), 1);
    auto first2 = await_value(context.core, first.get(), 2);

    gpu_signal(context.core, first, 2);
    EXPECT_TRUE(reached(first1));
    EXPECT_TRUE(reached(first2));
    EXPECT_TRUE(pending(first3));
    EXPECT_TRUE(pending(second1));

    gpu_signal(context.core, second, 1);
    EXPECT_TRUE(reached(second1));
    EXPECT_TRUE(pending(first3));

    gpu_signal(context.core, first, 3);
    EXPECT_TRUE(reached(first3));
}

JVK_TEST(TimelineReactor, host_signal_calls_the_callback) {
    test::Context const context{};
    cvk::TimelineSemaphore semaphore{context.core, cvk::create};

    auto result = await_value(context.core, semaphore.get(), 1);
    host_signal(semaphore, 1);

    ASSERT_TRUE(reached(result));
    EXPECT_FALSE(result.get().error);
}

JVK_TEST(TimelineReactor, smaller_value_wakes_the_thread) {
    test::Context const context{};
    cvk::TimelineSemaphore semaphore{context.core, cvk::create};

    // the thread waits for 5, the wait for 1 must reach it
    auto later = await_value(context.core, semaphore.get(), 5);
    auto earlier = await_value(context.core, semaphore.get(), 1);

    host_signal(semaphore, 1);
    EXPECT_TRUE(reached(earlier));
    EXPECT_TRUE(pending(later));

    host_signal(semaphore, 5);
    EXPECT_TRUE(reached(later));
}

JVK_TEST(TimelineReactor, many_waits_on_one_value) {
    constexpr size_t WAITS = 64;
    test::Context const context{};
    cvk::TimelineSemaphore const semaphore{context.core, cvk::create};

    std::vector<std::future<CallbackResult>> results{};
    for(size_t i = 0; i < WAITS; ++i)
        results.push_back(await_value(context.core, semaphore.get(), 1));

    gpu_signal(context.core, semaphore, 1);

    for(auto const& result : results)
        EXPECT_TRUE(reached(result));
}

JVK_TEST(TimelineReactor, reached_value_calls_the_callback) {
    test::Context const context{};
    cvk::TimelineSemaphore const semaphore{context.core, cvk::create, 3};

    auto result = await_value(context.core, semaphore.get(), 2);

    ASSERT_TRUE(reached(result));
    EXPECT_FALSE(result.get().error);
}
}
