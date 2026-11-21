#include "jvk/base/queue.hpp"

#include "context.hpp"
#include "test.hpp"

#include "jvk/data/buffer.hpp"
#include "jvk/data/image.hpp"

#include "cvk/base/queue/submit_info.hpp"
#include "cvk/render/sync/barrier.hpp"
#include "cvk/render/sync/fence.hpp"
#include "cvk/render/sync/timeline_semaphore.hpp"

#include <cth/coro/tasks/executor_task.hpp>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <thread>
#include <utility>
#include <vector>


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
     * @param[out] jobs
     */
    cth::co::executor_task<void> enter_rounds(
        Core const& core,
        Queue const& queue,
        StrandJobs& jobs
    ) {
        for(size_t round = 0; round < ROUNDS; ++round) {
            co_await queue.executor().schedule();

            if(!queue.executor().context().owns_thread())
                ++jobs.outsideStrand;
            auto const now = jobs.running.fetch_add(1) + 1;
            auto max = jobs.maxRunning.load();
            while(max < now && !jobs.maxRunning.compare_exchange_weak(max, now)) {}

            std::this_thread::sleep_for(std::chrono::microseconds{20});
            // widens the window for an overlap
            jobs.running.fetch_sub(1);

            co_await core.executor().schedule();
        }
    }

    using enum cvk::QueueFamilyProperties;

    constexpr VkExtent2D EXTENT{16, 16};
    constexpr size_t PIXELS = size_t{EXTENT.width} * EXTENT.height;

    constexpr cvk::Image::Config IMAGE_CONFIG{
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
    };

    constexpr cvk::BaseBuffer::Config DEVICE_CONFIG{
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
    };

    constexpr ImageState UNDEFINED{.layout = VK_IMAGE_LAYOUT_UNDEFINED, .scope = {}};
    constexpr BufferAccess HOST_READ{VK_PIPELINE_STAGE_2_HOST_BIT, VK_ACCESS_2_HOST_READ_BIT};
    constexpr cvk::BarrierScope TRANSFER_WRITE{
        VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        VK_ACCESS_2_TRANSFER_WRITE_BIT
    };
    constexpr cvk::BarrierScope TRANSFER_READ{
        VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        VK_ACCESS_2_TRANSFER_READ_BIT
    };

    /**
     * the test config with driver threads for the strands of the queues
     */
    Core::Config driver_threads() {
        auto config = test::Context::CONFIG;
        config.driverThreads = 2;
        return config;
    }

    /**
     * enters the strand of the queue
     * @return the strand owns the thread, the workers of the core own the thread
     */
    cth::co::executor_task<std::pair<bool, bool>> strand_thread(
        Core const& core,
        Queue const& queue
    ) {
        co_await queue.executor().schedule();
        co_return std::pair{
            queue.executor().context().owns_thread(),
            core.executor().context().owns_thread()
        };
    }

    /**
     * a graphics queue and a compute queue, on two families where the device has them
     */
    Core::Config two_families() {
        auto config = test::Context::CONFIG;
        config.vk.queueProperties = {GRAPHICS, COMPUTE};
        return config;
    }

    /**
     * staging buffer with one counting value per pixel
     */
    [[nodiscard]] Buffer<uint32_t> counting_staging(Core const& core) {
        Buffer<uint32_t> staging{core, test::STAGING_CONFIG, PIXELS};
        std::vector<uint32_t> values(PIXELS);
        std::iota(values.begin(), values.end(), uint32_t{0});
        staging.write(values);
        return staging;
    }

    /**
     * @return the readback holds the counting values of `counting_staging()`
     */
    [[nodiscard]] bool counts(cvk::Buffer<uint32_t> const& readback) {
        auto const mapping = readback.map();
        readback.invalidate();
        for(size_t i = 0; i < PIXELS; ++i)
            if(mapping[i] != i)
                return false;
        return true;
    }

    /**
     * the queue family ownership transfer of the image, without layout transition when the layouts are equal
     */
    [[nodiscard]] cvk::ImageBarrierInfo image_transfer(
        cvk::BarrierScope src,
        cvk::BarrierScope dst,
        VkImageLayout oldLayout,
        VkImageLayout newLayout
    ) {
        return cvk::ImageBarrierInfo{
            .src = src,
            .dst = dst,
            .newLayout = newLayout,
            .oldLayout = oldLayout
        };
    }

    /**
     * submits inside the strand
     */
    cth::co::executor_task<void> submit(
        Queue const& queue,
        cvk::SubmitInfo info,
        cvk::Fence const* fencePtr
    ) {
        co_await queue.executor().schedule();
        queue.vk().submit(info, fencePtr);
    }

    /**
     * submits inside the strand: signals the semaphore
     */
    cth::co::executor_task<void> signal(
        Queue const& queue,
        cvk::TimelineSemaphore const& semaphore
    ) {
        co_await queue.executor().schedule();
        queue.vk().submit({.signals = {cvk::SemaphoreSubmitInfo{semaphore}}});
    }
}

JVK_TEST(Queue, jobs_of_two_coroutines_never_overlap) {
    test::Context const context{};
    auto const& queue = context.core.queue(0);
    StrandJobs jobs{};

    std::jthread other{
        [&] { context.core.executor().sync_wait(enter_rounds(context.core, queue, jobs)); }
    };
    context.core.executor().sync_wait(enter_rounds(context.core, queue, jobs));
    other.join();

    EXPECT_EQ(jobs.outsideStrand.load(), 0);
    EXPECT_EQ(jobs.maxRunning.load(), 1);
}

JVK_TEST(Queue, strand_runs_on_the_workers_by_default) {
    test::Context const context{};

    auto const [inStrand, onWorker] =
        context.core.executor().sync_wait(strand_thread(context.core, context.core.queue(0)));
    EXPECT_TRUE(inStrand);
    EXPECT_TRUE(onWorker);
}

JVK_TEST(Queue, strand_runs_on_the_driver_threads) {
    test::Context const context{};
    Core const core{context.instance, driver_threads(), {}};

    auto const [inStrand, onWorker] = core.executor().sync_wait(strand_thread(core, core.queue(0)));
    EXPECT_TRUE(inStrand);
    EXPECT_FALSE(onWorker);
}

JVK_TEST(Queue, wait_idle_waits_for_the_submits) {
    test::Context const context{};
    auto const& queue = context.core.queue(0);
    cvk::TimelineSemaphore const semaphore{context.core, cvk::create};

    queue.executor().sync_wait(signal(queue, semaphore));
    queue.waitIdle();

    EXPECT_EQ(semaphore.gpuValue(), 1);
}

JVK_TEST(Queue, release_transfers_an_image_to_another_family) {
    test::Context const context{};
    Core const core{context.instance, two_families(), {}};
    auto const& owner = core.queue(0);
    auto const& receiver = core.queue(1);
    auto const ownerFamily = owner.vk().familyIndex();
    auto const receiverFamily = receiver.vk().familyIndex();
    if(ownerFamily == receiverFamily)
        GTEST_SKIP() << "needs two queue families, e.g. ANV_QUEUE_OVERRIDE=gc=2,c=1";

    Image const image{core, IMAGE_CONFIG, EXTENT};
    auto const staging = counting_staging(core);
    Buffer<uint32_t> const readback{core, test::READBACK_CONFIG, PIXELS};

    // written on the family of the owner
    PrimaryCmdBuffer write{core, ownerFamily};
    write.begin();
    write.assume(image, UNDEFINED);
    image.copy(write, staging);
    write.end();
    core.executor().sync_wait(submit(owner, {.cmdBuffers = {write}}, nullptr));

    // the release half moves the image to the layout of the read
    cvk::Barrier release{};
    release.add(
        image,
        image_transfer(
            {TRANSFER_WRITE.stages, TRANSFER_WRITE.access, ownerFamily},
            {.queueFamilyIndex = receiverFamily},
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
        )
    );
    auto const released = core.executor().sync_wait(owner.release(release));
    EXPECT_EQ(released.semaphore->submittedValue(), released.value);

    // the acquire half on the family of the receiver, then the read
    PrimaryCmdBuffer read{core, receiverFamily};
    read.begin();
    cvk::Barrier acquire{};
    acquire.add(
        image,
        image_transfer(
            {.stages = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, .queueFamilyIndex = ownerFamily},
            {TRANSFER_READ.stages, TRANSFER_READ.access, receiverFamily},
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
        )
    );
    acquire.execute(read);
    read.assume(
        image,
        {
            .layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            .scope = {TRANSFER_READ.stages, TRANSFER_READ.access}
        }
    );
    image.copyTo(read, readback);
    read.use(readback, HOST_READ);
    read.end();

    cvk::Fence const fence{core, cvk::create};
    core.executor().sync_wait(
        submit(receiver, {.cmdBuffers = {read}, .waits = {released}}, &fence)
    );
    fence.wait();

    EXPECT_TRUE(counts(readback));
}

JVK_TEST(Queue, several_releases_in_a_row) {
    constexpr size_t RELEASES = 4;
    test::Context const context{};
    Core const core{context.instance, two_families(), {}};
    auto const& owner = core.queue(0);
    auto const& receiver = core.queue(1);
    auto const ownerFamily = owner.vk().familyIndex();
    auto const receiverFamily = receiver.vk().familyIndex();
    if(ownerFamily == receiverFamily)
        GTEST_SKIP() << "needs two queue families, e.g. ANV_QUEUE_OVERRIDE=gc=2,c=1";

    auto const staging = counting_staging(core);
    std::vector<Buffer<uint32_t>> buffers{};
    std::vector<Buffer<uint32_t>> readbacks{};
    for(size_t i = 0; i < RELEASES; ++i) {
        buffers.emplace_back(core, DEVICE_CONFIG, PIXELS);
        readbacks.emplace_back(core, test::READBACK_CONFIG, PIXELS);
    }

    PrimaryCmdBuffer write{core, ownerFamily};
    write.begin();
    for(auto const& buffer : buffers)
        buffer.copy(write, staging);
    write.end();
    core.executor().sync_wait(submit(owner, {.cmdBuffers = {write}}, nullptr));

    // one release per buffer, each signals the next value of the counter
    std::vector<SemaphoreSubmitInfo> released{};
    for(auto const& buffer : buffers) {
        cvk::Barrier release{};
        release.add(
            buffer,
            cvk::BufferBarrierInfo::QueueTransition(
                {.queueFamilyIndex = receiverFamily},
                {TRANSFER_WRITE.stages, TRANSFER_WRITE.access, ownerFamily}
            )
        );
        released.push_back(core.executor().sync_wait(owner.release(release)));
    }
    for(size_t i = 1; i < RELEASES; ++i) {
        EXPECT_EQ(&released[i].semaphore.get(), &released[0].semaphore.get());
        EXPECT_EQ(released[i].value, released[i - 1].value + 1);
    }

    // the wait for the last value covers the earlier releases
    PrimaryCmdBuffer read{core, receiverFamily};
    read.begin();
    cvk::Barrier acquire{};
    for(auto const& buffer : buffers)
        acquire.add(
            buffer,
            cvk::BufferBarrierInfo::QueueTransition(
                {TRANSFER_READ.stages, TRANSFER_READ.access, receiverFamily},
                {.stages = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, .queueFamilyIndex = ownerFamily}
            )
        );
    acquire.execute(read);
    for(size_t i = 0; i < RELEASES; ++i) {
        read.assume(buffers[i], BufferState{.scope = {TRANSFER_READ.stages, TRANSFER_READ.access}});
        readbacks[i].copy(read, buffers[i]);
        read.use(readbacks[i], HOST_READ);
    }
    read.end();

    cvk::Fence const fence{core, cvk::create};
    core.executor().sync_wait(
        submit(receiver, {.cmdBuffers = {read}, .waits = {released.back()}}, &fence)
    );
    fence.wait();

    for(auto const& readback : readbacks)
        EXPECT_TRUE(counts(readback));

    // the gpu finished the releases: the next release reuses a command buffer
    receiver.waitIdle();
    owner.waitIdle();
    auto const next = core.executor().sync_wait(owner.release(cvk::Barrier{}));
    EXPECT_EQ(next.value, released.back().value + 1);
    owner.waitIdle();
}
}
