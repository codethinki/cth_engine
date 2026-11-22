#include "jvk/sync/timeline_reactor.hpp"

#include "context.hpp"
#include "test.hpp"

#include "jvk/base/queue.hpp"

#include "cvk/render/sync/timeline_semaphore.hpp"

#include <cth/coro/executor.hpp>
#include <cth/coro/scheduler.hpp>
#include <cth/coro/tasks/executor_task.hpp>
#include <cth/coro/utility/concepts.hpp>

#include <atomic>
#include <chrono>
#include <coroutine>
#include <cstddef>
#include <cstdint>
#include <future>
#include <memory>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>


namespace jvk {
namespace {
    using namespace std::chrono_literals;

    constexpr auto REACHED_TIMEOUT = 5s; ///< a reached value resumes its waiter before it
    constexpr auto PENDING_TIMEOUT = 50ms; ///< a pending waiter does not resume within it

    /**
     * what a wait got at its resume
     */
    struct WaitResult {
        std::error_code error;
        bool onWorker;
    };

    /**
     * awaiter of a wait of the reactor: the waiter lives in the frame of the caller
     */
    class reactor_wait : public cth::co::this_coro_awaiter_base {
    public:
        reactor_wait(
            TimelineReactor const& reactor,
            VkSemaphore vkSemaphore,
            uint64_t value,
            std::atomic<bool>& registered
        ) :
            _reactor{reactor},
            _vkSemaphore{vkSemaphore},
            _waiter{.value = value},
            _registered{registered} {}

        [[nodiscard]] static constexpr bool await_ready() noexcept { return false; }

        template<cth::co::this_coro_compatible Caller>
        void await_suspend(std::coroutine_handle<Caller> caller) {
            _waiter.caller = caller;
            _waiter.contextPtr = &caller.promise().context();
            // the resume can end the frame before await() returns: the flag lives outside of it
            auto& registered = _registered;
            _reactor.await(_vkSemaphore, _waiter);
            registered = true;
        }

        [[nodiscard]] std::error_code await_resume() const noexcept { return _waiter.error; }

    private:
        TimelineReactor const& _reactor;
        VkSemaphore _vkSemaphore;
        TimelineReactor::Waiter _waiter;
        std::atomic<bool>& _registered;
    };

    /**
     * waits with the reactor, then gives the result
     */
    cth::co::executor_task<void> wait_task(
        TimelineReactor const& reactor,
        cth::co::context const& workers,
        VkSemaphore vkSemaphore,
        uint64_t value,
        std::shared_ptr<std::atomic<bool>> registered,
        std::promise<WaitResult> promise
    ) {
        auto const error = co_await reactor_wait{reactor, vkSemaphore, value, *registered};
        promise.set_value({error, workers.owns_thread()});
    }

    /**
     * starts a wait of the reactor on the workers, returns once the reactor has the waiter
     * @return result of the wait
     */
    [[nodiscard]] std::future<WaitResult> await_value(
        TimelineReactor const& reactor,
        cth::co::context const& workers,
        VkSemaphore vkSemaphore,
        uint64_t value
    ) {
        auto registered = std::make_shared<std::atomic<bool>>(false);
        std::promise<WaitResult> promise{};
        auto future = promise.get_future();
        cth::co::executor{workers}.detach(
            wait_task(reactor, workers, vkSemaphore, value, registered, std::move(promise))
        );
        auto const end = std::chrono::steady_clock::now() + REACHED_TIMEOUT;
        while(!registered->load() && std::chrono::steady_clock::now() < end)
            std::this_thread::yield();
        EXPECT_TRUE(registered->load()) << "the reactor got no waiter";
        return future;
    }

    /**
     * a wait with the reactor of the core
     */
    [[nodiscard]] std::future<WaitResult> await_value(
        Core const& core,
        VkSemaphore vkSemaphore,
        uint64_t value
    ) {
        return await_value(core.reactor(), core.executor().context(), vkSemaphore, value);
    }

    [[nodiscard]] bool reached(std::future<WaitResult> const& future) {
        return future.wait_for(REACHED_TIMEOUT) == std::future_status::ready;
    }

    [[nodiscard]] bool pending(std::future<WaitResult> const& future) {
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

    TimelineReactor const reactor{context.core};

    EXPECT_FALSE(reactor.created());
}

JVK_TEST(TimelineReactor, create_and_destroy) {
    test::Context const context{};
    TimelineReactor reactor{context.core};

    reactor.create();
    EXPECT_TRUE(reactor.created());

    reactor.destroy();
    EXPECT_FALSE(reactor.created());

    reactor.create();
    EXPECT_TRUE(reactor.created());
}

JVK_TEST(TimelineReactor, a_moved_reactor_creates_and_resumes_a_waiter) {
    test::Context const context{};
    cth::co::scheduler workers{cth::co::autostart};
    cvk::TimelineSemaphore semaphore{context.core, cvk::create};
    TimelineReactor reactor{context.core};

    TimelineReactor moved{std::move(reactor)};
    TimelineReactor other{context.core};
    other = std::move(moved);
    other.create();
    EXPECT_TRUE(other.created());

    auto result = await_value(other, workers, semaphore.get(), 1);
    host_signal(semaphore, 1);

    ASSERT_TRUE(reached(result));
    auto const [error, onWorker] = result.get();
    EXPECT_FALSE(error);
    EXPECT_TRUE(onWorker);
}

JVK_TEST(TimelineReactor, gpu_signal_resumes_the_waiter_on_its_context) {
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

JVK_TEST(TimelineReactor, host_signal_resumes_the_waiter) {
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

    std::vector<std::future<WaitResult>> results{};
    for(size_t i = 0; i < WAITS; ++i)
        results.push_back(await_value(context.core, semaphore.get(), 1));

    gpu_signal(context.core, semaphore, 1);

    for(auto const& result : results)
        EXPECT_TRUE(reached(result));
}

JVK_TEST(TimelineReactor, a_reached_value_resumes_the_waiter) {
    test::Context const context{};
    cvk::TimelineSemaphore const semaphore{context.core, cvk::create, 3};

    auto result = await_value(context.core, semaphore.get(), 2);

    ASSERT_TRUE(reached(result));
    EXPECT_FALSE(result.get().error);
}
}
