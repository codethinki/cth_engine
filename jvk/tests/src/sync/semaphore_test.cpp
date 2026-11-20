#include "jvk/sync/semaphore.hpp"

#include "context.hpp"
#include "test.hpp"

#include "jvk/base/queue.hpp"
#include "jvk/sync/timeline_reactor.hpp"

#include "cvk/base/queue/submit_info.hpp"
#include "cvk/render/sync/timeline_semaphore.hpp"

#include <cth/coro/tasks/executor_task.hpp>

#include <chrono>
#include <cstdint>
#include <future>
#include <system_error>
#include <thread>
#include <utility>
#include <variant>


namespace jvk {
namespace {
    using namespace std::chrono_literals;

    constexpr auto REACHED_TIMEOUT = 5s; ///< a reached value resumes before it
    constexpr auto PENDING_TIMEOUT = 50ms; ///< a pending wait does not resume within it

    /**
     * signals the value on the gpu, submits inside the strand of the queue
     */
    cth::co::executor_task<void> gpu_signal(Queue const& queue, SemaphoreSubmitInfo signal) {
        co_await queue.executor().schedule();
        queue.vk().submit({.signals = {signal}});
    }

    void gpu_signal(Core const& core, Semaphore const& semaphore, uint64_t value) {
        core.executor().sync_wait(
            gpu_signal(core.queue(0), {.semaphore = semaphore, .value = value})
        );
    }

    /**
     * @return the coroutine resumed on a worker
     */
    cth::co::executor_task<bool> await_gpu(
        Core const& core,
        Semaphore const& semaphore,
        uint64_t value
    ) {
        co_await semaphore.wait(value);
        co_return core.executor().context().owns_thread();
    }

    /**
     * @return the coroutine resumed on a worker
     */
    cth::co::executor_task<bool> await_submitted(
        Core const& core,
        Semaphore const& semaphore,
        uint64_t value
    ) {
        co_await semaphore.submitted(value);
        co_return core.executor().context().owns_thread();
    }

    /**
     * runs the task on the workers, the calling thread does not block
     */
    template<class Task>
    [[nodiscard]] std::future<bool> start(Core const& core, Task task) {
        return std::async(
            std::launch::async,
            [&core, task = std::move(task)]() mutable {
                return core.executor().sync_wait(std::move(task));
            }
        );
    }

    /**
     * registers a callback for the submitted value
     * @return the callback ran
     */
    [[nodiscard]] std::future<std::error_code> await_mark(
        Semaphore const& semaphore,
        uint64_t value
    ) {
        std::promise<std::error_code> promise{};
        auto future = promise.get_future();
        semaphore.awaitSubmitted(
            value,
            [promise = std::move(promise)](std::error_code error) mutable {
                promise.set_value(error);
            }
        );
        return future;
    }

    template<class T>
    [[nodiscard]] bool reached(std::future<T> const& future) {
        return future.wait_for(REACHED_TIMEOUT) == std::future_status::ready;
    }

    template<class T>
    [[nodiscard]] bool pending(std::future<T> const& future) {
        return future.wait_for(PENDING_TIMEOUT) == std::future_status::timeout;
    }

    /**
     * blocks until the reactor of the core has a callback for the semaphore
     */
    [[nodiscard]] bool await_registration(Core const& core, Semaphore const& semaphore) {
        auto const end = std::chrono::steady_clock::now() + REACHED_TIMEOUT;
        while(!core.reactor().pending(semaphore.get()))
            if(std::chrono::steady_clock::now() > end)
                return false;
            else
                std::this_thread::sleep_for(1ms);
        return true;
    }
}

JVK_TEST(Semaphore, base_constructor_does_not_create) {
    test::Context const context{};

    Semaphore const semaphore{context.core};

    EXPECT_FALSE(semaphore.created());
    EXPECT_EQ(&semaphore.core(), &context.core);
}

JVK_TEST(Semaphore, create_with_an_initial_value) {
    test::Context const context{};

    Semaphore const semaphore{context.core, cvk::create, 5};

    ASSERT_TRUE(semaphore.created());
    EXPECT_EQ(semaphore.value(), 5);
    EXPECT_EQ(semaphore.gpuValue(), 5);
    EXPECT_EQ(semaphore.submittedValue(), 5);
}

JVK_TEST(Semaphore, destroy_and_create_again) {
    test::Context const context{};
    Semaphore semaphore{context.core, cvk::create};

    semaphore.destroy();
    EXPECT_FALSE(semaphore.created());

    semaphore.create(2);
    ASSERT_TRUE(semaphore.created());
    EXPECT_EQ(semaphore.submittedValue(), 2);
}

JVK_TEST(Semaphore, release_and_wrap) {
    test::Context const context{};
    Semaphore semaphore{context.core, cvk::create, 4};
    auto const vkSemaphore = semaphore.get();

    auto const state = semaphore.release();
    EXPECT_FALSE(semaphore.created());
    EXPECT_EQ(state.vkSemaphore.get(), vkSemaphore);
    EXPECT_EQ(state.value, 4);

    Semaphore const wrapped{context.core, state};
    EXPECT_EQ(wrapped.get(), vkSemaphore);
    EXPECT_EQ(wrapped.value(), 4);
    EXPECT_EQ(wrapped.submittedValue(), 4);
}

JVK_TEST(Semaphore, move_keeps_the_semaphore_and_the_submitted_value) {
    test::Context const context{};
    Semaphore semaphore{context.core, cvk::create};
    semaphore.markSubmitted(3);
    auto const vkSemaphore = semaphore.get();

    Semaphore const moved{std::move(semaphore)};

    EXPECT_EQ(moved.get(), vkSemaphore);
    EXPECT_EQ(moved.submittedValue(), 3);
}

JVK_TEST(Semaphore, converts_to_a_cvk_semaphore_submit_info) {
    test::Context const context{};
    Semaphore const semaphore{context.core, cvk::create};
    cvk::TimelineSemaphore const& vk = semaphore;

    cvk::SemaphoreSubmitInfo const info{semaphore, VK_PIPELINE_STAGE_2_TRANSFER_BIT, 3};
    cvk::SemaphoreSubmitInfo const converted = SemaphoreSubmitInfo{
        semaphore,
        VK_PIPELINE_STAGE_2_COPY_BIT,
        4
    };

    using timeline_t = cth::ref_wrapper<cvk::TimelineSemaphore const>;
    ASSERT_TRUE(std::holds_alternative<timeline_t>(info.semaphore));
    EXPECT_EQ(&std::get<timeline_t>(info.semaphore).get(), &vk);
    EXPECT_EQ(info.value, 3);

    ASSERT_TRUE(std::holds_alternative<timeline_t>(converted.semaphore));
    EXPECT_EQ(&std::get<timeline_t>(converted.semaphore).get(), &vk);
    EXPECT_EQ(converted.stages, VK_PIPELINE_STAGE_2_COPY_BIT);
    EXPECT_EQ(converted.value, 4);
}

JVK_TEST(Semaphore, gpu_signal_of_a_jvk_submit_info) {
    test::Context const context{};
    Semaphore const semaphore{context.core, cvk::create};

    gpu_signal(context.core, semaphore, 2);
    static_cast<cvk::TimelineSemaphore const&>(semaphore).wait(2);

    EXPECT_EQ(semaphore.gpuValue(), 2);
}

JVK_TEST(Semaphore, wait_resumes_on_a_worker_after_the_gpu_signal) {
    test::Context const context{};
    Semaphore const semaphore{context.core, cvk::create};

    auto resumed = start(context.core, await_gpu(context.core, semaphore, 1));
    EXPECT_TRUE(await_registration(context.core, semaphore));
    EXPECT_TRUE(pending(resumed));

    gpu_signal(context.core, semaphore, 1);

    ASSERT_TRUE(reached(resumed));
    EXPECT_TRUE(resumed.get());
}

JVK_TEST(Semaphore, reached_values_do_not_suspend) {
    test::Context const context{};
    Semaphore const semaphore{context.core, cvk::create, 5};

    EXPECT_TRUE(semaphore.wait(3).await_ready());
    EXPECT_TRUE(semaphore.wait(5).await_ready());
    EXPECT_FALSE(semaphore.wait(6).await_ready());

    EXPECT_TRUE(semaphore.submitted(5).await_ready());
    EXPECT_FALSE(semaphore.submitted(6).await_ready());

    EXPECT_TRUE(context.core.executor().sync_wait(await_gpu(context.core, semaphore, 4)));
    EXPECT_TRUE(context.core.executor().sync_wait(await_submitted(context.core, semaphore, 4)));
}

JVK_TEST(Semaphore, mark_submitted_calls_the_waiters_up_to_the_value) {
    test::Context const context{};
    Semaphore const semaphore{context.core, cvk::create};

    auto third = await_mark(semaphore, 3);
    auto first = await_mark(semaphore, 1);
    auto second = await_mark(semaphore, 2);

    semaphore.markSubmitted(2);
    EXPECT_EQ(semaphore.submittedValue(), 2);
    ASSERT_TRUE(reached(first));
    ASSERT_TRUE(reached(second));
    EXPECT_FALSE(first.get());
    EXPECT_FALSE(second.get());
    EXPECT_TRUE(pending(third));

    semaphore.markSubmitted(3);
    ASSERT_TRUE(reached(third));
    EXPECT_FALSE(third.get());
}

JVK_TEST(Semaphore, marked_value_calls_the_callback_at_once) {
    test::Context const context{};
    Semaphore const semaphore{context.core, cvk::create};
    semaphore.markSubmitted(2);

    auto result = await_mark(semaphore, 1);

    EXPECT_TRUE(reached(result));
}

JVK_TEST(Semaphore, submitted_resumes_on_a_worker_after_the_mark) {
    test::Context const context{};
    Semaphore const semaphore{context.core, cvk::create};

    auto resumed = start(context.core, await_submitted(context.core, semaphore, 1));
    EXPECT_TRUE(pending(resumed));

    semaphore.markSubmitted(1);

    ASSERT_TRUE(reached(resumed));
    EXPECT_TRUE(resumed.get());
}
}
