#include "jvk/base/core.hpp"

#include "context.hpp"
#include "test.hpp"

#include "jvk/base/queue.hpp"
#include "jvk/base/queue_sets.hpp"
#include "jvk/data/sampler_heap.hpp"
//TEMP compile check of the phase 6 headers until their sources exist
#include "jvk/graph/surface.hpp"
#include "jvk/graph/stage_graph.hpp"
#include "jvk/sync/semaphore.hpp"
#include "jvk/sync/timeline_reactor.hpp"

#include "cvk/base/device.hpp"
#include "cvk/base/physical_device.hpp"
#include "cvk/data/img/sampler_config.hpp"
#include "cvk/render/sync/fence.hpp"
#include "cvk/render/sync/timeline_semaphore.hpp"
#include "cvk/utility/constant/os_constants.hpp"
#include "cvk/utility/vk_exceptions.hpp"

#include <cth/coro/tasks/executor_task.hpp>
#include <cth/coro/this_coro.hpp>
#include <cth/os/native_handle.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <future>
#include <numeric>
#include <optional>
#include <system_error>
#include <thread>
#include <utility>


namespace jvk {
namespace {
    using enum cvk::QueueFamilyProperties;

    /**
     * two queues, default queue sets
     */
    Core::Config two_queues() {
        auto config = test::Context::CONFIG;
        config.vk.queueProperties = {GRAPHICS, COMPUTE};
        return config;
    }

    /**
     * every queue in one group
     */
    cvk::Core::queue_set_t one_group(size_t queueCount) {
        cvk::Core::queue_set_t::group_t group(queueCount);
        std::iota(group.begin(), group.end(), 0uz);
        return cvk::Core::queue_set_t{queueCount, {group}};
    }

    /**
     * @return the coroutine runs on a worker of the core
     */
    cth::co::executor_task<bool> on_worker(Core const& core) {
        co_return core.executor().context().owns_thread();
    }

    /**
     * the config of the test context with `cvk::constants::OS_FENCE_DEVICE_EXTENSIONS`
     */
    Core::Config os_fence_config() {
        auto config = test::Context::CONFIG;
        for(auto const extension : cvk::constants::OS_FENCE_DEVICE_EXTENSIONS)
            config.vk.extensions.emplace_back(extension);
        return config;
    }

    /**
     * @return the device has `cvk::constants::OS_FENCE_FEATURES`
     */
    bool os_fence_exportable(cvk::Core const& core) {
        auto const features = core.physicalDevice().externalFenceFeatures(
            cvk::constants::OS_FENCE_HANDLE_TYPE
        );
        return (features & cvk::constants::OS_FENCE_FEATURES) == cvk::constants::OS_FENCE_FEATURES;
    }

    /**
     * submits inside the strand of the queue: signals the semaphore and the fence
     * @return the os handle of the fence
     */
    cth::co::executor_task<cth::os::unique_native_handle> submit(
        Queue const& queue,
        cvk::TimelineSemaphore const& semaphore,
        cvk::Fence const& fence
    ) {
        co_await queue.executor().schedule();
        queue.vk().submit({.signals = {cvk::SemaphoreSubmitInfo{semaphore}}}, &fence);
        co_return fence.extractOsHandle();
    }

    struct resume_result {
        bool onWorker;
        uint64_t gpuValue;
    };

    /**
     * submits inside the strand, awaits the os handle of the fence on the worker
     */
    cth::co::executor_task<resume_result> submit_and_await(
        Core const& core,
        cvk::TimelineSemaphore const& semaphore,
        cvk::Fence const& fence
    ) {
        auto const handle = co_await submit(core.queue(0), semaphore, fence);
        // invalid handle -> already signaled
        if(handle)
            co_await cth::co::this_coro::await_handle(handle.get());

        co_return resume_result{core.executor().context().owns_thread(), semaphore.gpuValue()};
    }
}

JVK_TEST(Core, base_constructor_does_not_create) {
    test::Context const context{};

    Core const core{context.instance, test::Context::CONFIG};

    EXPECT_FALSE(core.created());
    EXPECT_FALSE(static_cast<cvk::Core const&>(core).created());
    EXPECT_EQ(core.config().workers, test::Context::CONFIG.workers);
    EXPECT_EQ(core.config().driverThreads, test::Context::CONFIG.driverThreads);
}

JVK_TEST(Core, a_base_constructed_core_moves_then_creates_and_works) {
    test::Context const context{};
    Core core{context.instance, test::Context::CONFIG};

    Core moved{std::move(core)};
    Core other{context.instance, test::Context::CONFIG};
    other = std::move(moved);
    EXPECT_FALSE(other.created());

    other.create({});
    ASSERT_TRUE(other.created());

    // the queues, the reactor and the workers of the moved core
    Semaphore const semaphore{other, cvk::create};
    auto task = [](Core const& c, Semaphore const& s) -> cth::co::executor_task<bool> {
        co_await c.queue(0).submit({.signals = {{.semaphore = s, .value = 1}}});
        co_await s.wait(1);
        co_return c.executor().context().owns_thread();
    };
    EXPECT_TRUE(other.executor().sync_wait(task(other, semaphore)));

    auto const& heaps = other.config().heaps;
    if(heaps.createsResourceHeap())
        EXPECT_TRUE(other.resourceHeap().created());
    if(heaps.createsSamplerHeap())
        EXPECT_EQ(other.samplerHeap().insert(cvk::SamplerConfig{}).idx().get(), 0);
}

JVK_TEST(Core, create_and_destroy) {
    test::Context const context{};
    Core core{context.instance, test::Context::CONFIG};
    cvk::Core const& vk = core;

    core.create({});
    ASSERT_TRUE(core.created());
    EXPECT_TRUE(vk.created());
    EXPECT_TRUE(core.executor().sync_wait(on_worker(core)));
    core.queue(0).waitIdle();

    core.destroy();
    EXPECT_FALSE(core.created());
    EXPECT_FALSE(vk.created());

    core.create({});
    ASSERT_TRUE(core.created());
    EXPECT_TRUE(core.executor().sync_wait(on_worker(core)));
}

JVK_TEST(Core, create_with_a_surface) {
    test::Context const context{};
    auto const surface = test::headless_surface(context.instance);
    Core::Config config = test::Context::CONFIG;
    config.vk.queueProperties = {GRAPHICS | PRESENT};

    std::optional<Core> core{};
    try {
        core.emplace(context.instance, std::move(config), std::array{cth::ref_wrapper{surface}});
    }
    catch(cvk::cvk_exception const&) {
        GTEST_SKIP() << "no device presents to a headless surface";
    }
    cvk::Core const& vk = *core;

    ASSERT_TRUE(core->created());
    EXPECT_TRUE(vk.queue(0).canPresent());
    EXPECT_TRUE(core->reactor().created());
}

JVK_TEST(Core, create_with_a_braced_surface_list) {
    test::Context const context{};
    auto const surface = test::headless_surface(context.instance);
    Core::Config config = test::Context::CONFIG;
    config.vk.queueProperties = {GRAPHICS | PRESENT};
    Core core{context.instance, std::move(config)};

    try { core.create({surface}); }
    catch(cvk::cvk_exception const&) {
        GTEST_SKIP() << "no device presents to a headless surface";
    }

    cvk::Core const& vk = core;
    EXPECT_TRUE(vk.queue(0).canPresent());
}

JVK_TEST(Core, reactor_waits_after_a_new_create) {
    test::Context const context{};
    Core core{context.instance, test::Context::CONFIG, {}};

    for(size_t round = 0; round < 2; ++round) {
        ASSERT_TRUE(core.reactor().created());
        Semaphore semaphore{core, cvk::create};

        auto wait = [](Semaphore const& s) -> cth::co::executor_task<void> { co_await s.wait(1); };
        auto result = std::async(
            std::launch::async,
            [&core, &semaphore, wait] { core.executor().sync_wait(wait(semaphore)); }
        );
        auto const end = std::chrono::steady_clock::now() + std::chrono::seconds{5};
        while(!core.reactor().pending(semaphore.get()) && std::chrono::steady_clock::now() < end)
            std::this_thread::yield();
        ASSERT_TRUE(core.reactor().pending(semaphore.get()));

        core.executor().sync_wait(
            core.queue(0).submit({.signals = {{.semaphore = semaphore, .value = 1}}})
        );
        ASSERT_EQ(result.wait_for(std::chrono::seconds{5}), std::future_status::ready);
        result.get();

        semaphore.destroy();
        core.destroy();
        core.create({});
    }
}

JVK_TEST(Core, enables_the_extensions_of_the_config) {
    test::Context const context{};
    std::optional<Core> core{};
    try { core.emplace(context.instance, os_fence_config(), test::NO_SURFACES); }
    catch(cvk::cvk_exception const&) { GTEST_SKIP() << "no os fence extension"; }
    cvk::Core const& vk = *core;

    auto const extensions = vk.device().extensions();

    for(auto const extension : cvk::constants::OS_FENCE_DEVICE_EXTENSIONS)
        EXPECT_TRUE(std::ranges::contains(extensions, extension)) << extension;
}

JVK_TEST(Core, default_queue_sets) {
    test::Context const context{};

    // without queue sets a device with one queue has no vulkan queue for each queue of the config
    Core const core{context.instance, two_queues(), {}};
    cvk::Core const& vk = core;

    ASSERT_TRUE(core.created());
    auto const sets = queue_sets(core.config().vk.queueProperties);
    ASSERT_LT(vk.queueSetIndex(), sets.size());
    auto const& set = sets[vk.queueSetIndex()];
    EXPECT_EQ(vk.queueIndex(0) == vk.queueIndex(1), set.find(0) == set.find(1));
}

JVK_TEST(Core, indices_of_one_vulkan_queue_share_a_queue) {
    test::Context const context{};
    auto config = two_queues();
    config.vk.queueSets = {one_group(config.vk.queueProperties.size())};

    Core const core{context.instance, std::move(config), {}};

    EXPECT_EQ(&core.queue(0), &core.queue(1));
}

JVK_TEST(Core, one_queue_per_vulkan_queue) {
    test::Context const context{};

    Core const core{context.instance, two_queues(), {}};
    cvk::Core const& vk = core;

    for(size_t queueIdx = 0; queueIdx < 2; ++queueIdx) {
        EXPECT_EQ(&core.queue(queueIdx).vk(), &vk.queue(queueIdx));
        EXPECT_EQ(core.queue(queueIdx).get(), vk.queue(queueIdx).get());
    }
    EXPECT_EQ(&core.queue(0) == &core.queue(1), vk.queueIndex(0) == vk.queueIndex(1));
}

JVK_TEST(Core, executor_runs_on_the_workers) {
    test::Context const context{};

    EXPECT_FALSE(context.core.executor().context().owns_thread());
    EXPECT_TRUE(context.core.executor().sync_wait(on_worker(context.core)));
}

JVK_TEST(Core, worker_resumes_after_the_gpu_signals_the_fence_of_a_strand_submit) {
    test::Context const context{};
    std::optional<Core> core{};
    try { core.emplace(context.instance, os_fence_config(), test::NO_SURFACES); }
    catch(cvk::cvk_exception const&) { GTEST_SKIP() << "no os fence extension"; }
    if(!os_fence_exportable(*core))
        GTEST_SKIP() << "os fence handle type not exportable";

    cvk::TimelineSemaphore const semaphore{*core, cvk::create};
    cvk::Fence const fence{*core, cvk::create, 0, cvk::constants::OS_FENCE_HANDLE_TYPE};

    auto const [onWorker, gpuValue] = core->executor().sync_wait(
        submit_and_await(*core, semaphore, fence)
    );

    EXPECT_TRUE(onWorker);
    EXPECT_EQ(gpuValue, 1);

    core->queue(0).waitIdle();
}
}
