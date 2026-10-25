#include "jvk/base/core.hpp"

#include "context.hpp"
#include "test.hpp"

#include "jvk/base/queue.hpp"
#include "jvk/base/queue_sets.hpp"

#include "cvk/base/device.hpp"
#include "cvk/base/physical_device.hpp"
#include "cvk/render/sync/fence.hpp"
#include "cvk/render/sync/timeline_semaphore.hpp"
#include "cvk/utility/constant/os_constants.hpp"

#include <cth/coro/tasks/executor_task.hpp>
#include <cth/coro/this_coro.hpp>
#include <cth/os/native_handle.hpp>

#include <algorithm>
#include <cstdint>
#include <numeric>
#include <utility>


namespace jvk {
namespace {
    using enum cvk::QueueFamilyProperties;

    /**
     * two roles, default queue sets
     */
    Core::Config two_roles() {
        auto config = test::Context::CONFIG;
        config.vk.queueProperties = {GRAPHICS, COMPUTE};
        return config;
    }

    /**
     * every role in one group
     */
    cvk::Core::queue_set_t one_group(size_t roles) {
        cvk::Core::queue_set_t::group_t group(roles);
        std::iota(group.begin(), group.end(), 0uz);
        return cvk::Core::queue_set_t{roles, {group}};
    }

    /**
     * @return the coroutine runs on a worker of the core
     */
    cth::co::executor_task<bool> on_worker(Core const& core) {
        co_return core.executor().context().owns_thread();
    }

    /**
     * @return the device can export @ref cvk::constants::OS_FENCE_HANDLE_TYPE
     */
    bool os_fence_exportable(cvk::Core const& core) {
        VkPhysicalDeviceExternalFenceInfo const info{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_FENCE_INFO,
            .pNext = nullptr,
            .handleType = cvk::constants::OS_FENCE_HANDLE_TYPE,
        };
        VkExternalFenceProperties properties{.sType = VK_STRUCTURE_TYPE_EXTERNAL_FENCE_PROPERTIES, .pNext = nullptr};
        vkGetPhysicalDeviceExternalFenceProperties(core.vkPhysicalDevice(), &info, &properties);
        return (properties.externalFenceFeatures & VK_EXTERNAL_FENCE_FEATURE_EXPORTABLE_BIT) != 0;
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
        if(handle) co_await cth::co::this_coro::await_handle(handle.get());

        co_return resume_result{core.executor().context().owns_thread(), semaphore.gpuValue()};
    }
}

JVK_TEST(Core, base_constructor_does_not_create) {
    test::Context const context{};

    Core const core{context.instance, test::Context::CONFIG};

    EXPECT_FALSE(core.created());
    EXPECT_FALSE(static_cast<cvk::Core const&>(core).created());
    EXPECT_EQ(core.config().workers, test::Context::CONFIG.workers);
    EXPECT_EQ(core.config().queueWorkers, test::Context::CONFIG.queueWorkers);
}

JVK_TEST(Core, create_and_destroy) {
    test::Context const context{};
    Core core{context.instance, test::Context::CONFIG};
    cvk::Core const& vk = core;

    core.create();
    ASSERT_TRUE(core.created());
    EXPECT_TRUE(vk.created());
    EXPECT_TRUE(core.executor().sync_wait(on_worker(core)));
    core.queue(0).waitIdle();

    core.destroy();
    EXPECT_FALSE(core.created());
    EXPECT_FALSE(vk.created());

    core.create();
    ASSERT_TRUE(core.created());
    EXPECT_TRUE(core.executor().sync_wait(on_worker(core)));
}

JVK_TEST(Core, enables_the_os_fence_extensions) {
    test::Context const context{};
    cvk::Core const& vk = context.core;

    auto const extensions = vk.device().extensions();

    for(auto const extension : cvk::constants::OS_FENCE_DEVICE_EXTENSIONS)
        EXPECT_TRUE(std::ranges::contains(extensions, extension)) << extension;
}

JVK_TEST(Core, default_queue_sets) {
    test::Context const context{};

    // without queue sets a device with one queue has no queue for each role
    Core const core{context.instance, two_roles(), cvk::create};
    cvk::Core const& vk = core;

    ASSERT_TRUE(core.created());
    auto const sets = queue_sets(core.config().vk.queueProperties);
    ASSERT_LT(vk.queueSetIndex(), sets.size());
    auto const& set = sets[vk.queueSetIndex()];
    EXPECT_EQ(vk.queueIndex(0) == vk.queueIndex(1), set.find(0) == set.find(1));
}

JVK_TEST(Core, roles_with_the_same_vulkan_queue_share_a_queue) {
    test::Context const context{};
    auto config = two_roles();
    config.vk.queueSets = {one_group(config.vk.queueProperties.size())};

    Core const core{context.instance, std::move(config), cvk::create};

    EXPECT_EQ(&core.queue(0), &core.queue(1));
}

JVK_TEST(Core, one_queue_per_vulkan_queue) {
    test::Context const context{};

    Core const core{context.instance, two_roles(), cvk::create};
    cvk::Core const& vk = core;

    for(size_t role = 0; role < 2; ++role) {
        EXPECT_EQ(&core.queue(role).vk(), &vk.queue(role));
        EXPECT_EQ(core.queue(role).get(), vk.queue(role).get());
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
    if(!os_fence_exportable(context.core)) GTEST_SKIP() << "os fence handle type not exportable";

    cvk::TimelineSemaphore const semaphore{context.core, cvk::create};
    cvk::Fence const fence{context.core, cvk::create, 0, cvk::constants::OS_FENCE_HANDLE_TYPE};

    auto const [onWorker, gpuValue] =
        context.core.executor().sync_wait(submit_and_await(context.core, semaphore, fence));

    EXPECT_TRUE(onWorker);
    EXPECT_EQ(gpuValue, 1);

    context.core.queue(0).waitIdle();
}
}
