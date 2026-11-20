#include "jvk/graph/stage_graph.hpp"

#include "context.hpp"
#include "test.hpp"

#include "jvk/base/queue.hpp"
#include "jvk/data/buffer.hpp"
#include "jvk/data/image.hpp"

#include "cvk/base/allocator.hpp"
#include "cvk/base/queue/submit_info.hpp"
#include "cvk/render/sync/barrier.hpp"
#include "cvk/render/sync/semaphore.hpp"

#include <volk.h>
#include <vk_mem_alloc.h>

#include <cth/coro/tasks/executor_task.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <optional>
#include <semaphore>
#include <thread>
#include <utility>
#include <vector>


namespace jvk {
namespace {
    using enum cvk::QueueFamilyProperties;

    constexpr size_t VALUES = 64;

    constexpr auto TIMEOUT = std::chrono::seconds{5};

    constexpr cvk::BaseBuffer::Config DEVICE_CONFIG{
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
    };

    /**
     * one pixel per value
     */
    constexpr VkExtent2D EXTENT{8, 8};

    constexpr cvk::Image::Config IMAGE_CONFIG{
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
    };

    constexpr BufferAccess HOST_READ{VK_PIPELINE_STAGE_2_HOST_BIT, VK_ACCESS_2_HOST_READ_BIT};

    constexpr StageWait TRANSFER_WAIT_ON_0{
        .stageId = 0,
        .stages = VK_PIPELINE_STAGE_2_TRANSFER_BIT
    };

    /**
     * rendering without attachments
     */
    constexpr VkRect2D AREA{.offset = {0, 0}, .extent = EXTENT};

    struct test_exception {};

    /**
     * two graphics queues, two vulkan queues where the device has them
     */
    Core::Config two_queues() {
        auto config = test::Context::CONFIG;
        config.vk.queueProperties = {GRAPHICS, GRAPHICS};
        return config;
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
     * @return number of the vma allocations of the core
     */
    [[nodiscard]] uint32_t allocations(Core const& core) {
        cvk::Core const& vk = core;
        VmaTotalStatistics statistics{};
        vmaCalculateStatistics(vk.allocator().get(), &statistics);
        return statistics.total.statistics.allocationCount;
    }

    /**
     * @return the number of vma allocations reached the count before the timeout
     */
    [[nodiscard]] bool reaches_allocations(Core const& core, uint32_t count) {
        auto const deadline = std::chrono::steady_clock::now() + TIMEOUT;
        while(allocations(core) != count) {
            if(std::chrono::steady_clock::now() > deadline)
                return false;
            std::this_thread::sleep_for(std::chrono::milliseconds{1});
        }
        return true;
    }

    /**
     * signals the semaphore from the host, without a mark
     */
    void host_signal(Semaphore const& semaphore, uint64_t value) {
        cvk::Core const& vk = semaphore.core();
        VkSemaphoreSignalInfo const info{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO,
            .pNext = nullptr,
            .semaphore = semaphore.get(),
            .value = value,
        };
        ASSERT_EQ(vk.functions()->vkSignalSemaphore(vk.vkDevice(), &info), VK_SUCCESS);
    }

    [[nodiscard]] Buffer<uint32_t> counting_staging(Core const& core) {
        Buffer<uint32_t> staging{core, test::STAGING_CONFIG, VALUES};
        std::vector<uint32_t> values(VALUES);
        std::iota(values.begin(), values.end(), uint32_t{0});
        staging.write(values);
        return staging;
    }

    [[nodiscard]] bool counts(cvk::Buffer<uint32_t> const& readback) {
        auto const mapping = readback.map();
        readback.invalidate();
        for(size_t i = 0; i < VALUES; ++i)
            if(mapping[i] != i)
                return false;
        return true;
    }

    Frame& next_frame(Core const& core, StageGraph& graph) {
        return core.executor().sync_wait(graph.nextFrame());
    }

    cth::co::executor_task<void> await_stage(Stage const& stage) { co_await stage; }

    cth::co::executor_task<void> await_submitted(Stage const& stage) { co_await stage.submitted(); }

    /**
     * enters the strand of the queue: the jobs posted to it before ended
     */
    cth::co::executor_task<void> enter_strand(Queue const& queue) {
        co_await queue.executor().schedule();
    }

    /**
     * blocks until the gpu finished the stage
     */
    void wait(Core const& core, Stage const& stage) {
        core.executor().sync_wait(await_stage(stage));
    }

    /**
     * submits an empty primary at the end of the scope
     */
    void submit_empty(Stage& stage) { auto const primary = stage.begin(); }

    /**
     * submits inside the strand of the queue
     */
    cth::co::executor_task<void> submit(Queue const& queue, cvk::SubmitInfo info) {
        co_await queue.executor().schedule();
        queue.vk().submit(info);
    }
}

JVK_TEST(StageGraph, first_frame_number_is_above_the_largest_frame_offset) {
    test::Context const context{};
    Core core{context.instance, test::Context::CONFIG, {}};

    StageGraph const plain{core, {.stages = {{.queueIdx = 0}}}};
    StageGraph const offset{
        core,
        {.stages = {{.queueIdx = 0}, {.queueIdx = 0, .waits = {{.stageId = 0, .frameOffset = 3}}}}},
    };

    EXPECT_EQ(plain.firstFrameNumber(), 1);
    EXPECT_EQ(offset.firstFrameNumber(), 4);
}

JVK_TEST(StageGraph, one_stage_over_many_frames) {
    constexpr size_t FRAMES = 7;
    test::Context const context{};
    Core core{context.instance, test::Context::CONFIG, {}};
    StageGraph graph{core, {.stages = {{.queueIdx = 0}}, .frameSlots = 2}};

    std::vector<PrimaryCmdBuffer const*> primaries{};
    for(size_t i = 0; i < FRAMES; ++i) {
        auto& frame = next_frame(core, graph);
        auto& stage = frame.stage(0);

        EXPECT_EQ(frame.number(), graph.firstFrameNumber() + i);
        EXPECT_EQ(frame.slot(), i % 2);
        EXPECT_EQ(stage.frameNumber(), frame.number());
        EXPECT_EQ(stage.status(), Stage::Status::UNSUBMITTED);

        primaries.push_back(&stage.primary());
        submit_empty(stage);
        EXPECT_EQ(stage.status(), Stage::Status::SUBMITTED);

        if(i + 1 == FRAMES) {
            wait(core, stage);
            EXPECT_GE(stage.semaphore().gpuValue(), stage.frameNumber());
        }
    }

    // the slots come back
    for(size_t i = 2; i < FRAMES; ++i)
        EXPECT_EQ(primaries[i], primaries[i - 2]);
    EXPECT_NE(primaries[0], primaries[1]);
}

JVK_TEST(StageGraph, chain_on_one_queue_submitted_in_reverse) {
    test::Context const context{};
    Core core{context.instance, test::Context::CONFIG, {}};
    StageGraph graph{
        core,
        {.stages = {{.queueIdx = 0}, {.queueIdx = 0, .waits = {TRANSFER_WAIT_ON_0}}}}
    };

    auto const staging = counting_staging(core);
    Buffer<uint32_t> const buffer{core, DEVICE_CONFIG, VALUES};
    Buffer<uint32_t> const readback{core, test::READBACK_CONFIG, VALUES};

    auto& frame = next_frame(core, graph);
    auto& writer = frame.stage(0);
    auto& reader = frame.stage(1);

    // the reader submits first, from another thread: its job waits for the mark of the writer
    std::jthread{
        [&] {
            auto const primary = reader.begin();
            readback.copy(*primary, buffer);
            primary->use(readback, HOST_READ);
            reader.submit();
        }
    }.join();

    auto const primary = writer.begin();
    buffer.copy(*primary, staging);
    writer.submit();

    wait(core, reader);
    EXPECT_TRUE(counts(readback));
}

JVK_TEST(StageGraph, chain_on_two_queues) {
    test::Context const context{};
    Core core{context.instance, two_queues(), {}};
    if(&core.queue(0) == &core.queue(1))
        GTEST_SKIP() << "needs two vulkan queues, e.g. ANV_QUEUE_OVERRIDE=gc=2,c=1";
    StageGraph graph{
        core,
        {.stages = {{.queueIdx = 0}, {.queueIdx = 1, .waits = {TRANSFER_WAIT_ON_0}}}}
    };

    auto const staging = counting_staging(core);
    Buffer<uint32_t> const buffer{core, DEVICE_CONFIG, VALUES};
    Buffer<uint32_t> const readback{core, test::READBACK_CONFIG, VALUES};

    auto& frame = next_frame(core, graph);
    auto& writer = frame.stage(0);
    auto& reader = frame.stage(1);

    // another queue: the reader submits at once, the gpu waits
    auto const readerPrimary = reader.begin();
    readback.copy(*readerPrimary, buffer);
    readerPrimary->use(readback, HOST_READ);
    reader.submit();

    auto const writerPrimary = writer.begin();
    buffer.copy(*writerPrimary, staging);
    writer.submit();

    wait(core, reader);
    EXPECT_TRUE(counts(readback));
}

JVK_TEST(StageGraph, frame_offset_wait_is_reached_in_the_first_frames) {
    constexpr size_t FRAMES = 5;
    test::Context const context{};
    Core core{context.instance, test::Context::CONFIG, {}};
    StageGraph graph{
        core,
        {
            .stages = {
                {.queueIdx = 0},
                {.queueIdx = 0, .waits = {{.stageId = 0, .frameOffset = 1}}}
            },
            .frameSlots = 2
        },
    };
    ASSERT_EQ(graph.firstFrameNumber(), 2);

    Stage const* lastPtr = nullptr;
    for(size_t i = 0; i < FRAMES; ++i) {
        auto& frame = next_frame(core, graph);

        // the waiter first: in the first frame it waits for frame 1, before the first frame
        submit_empty(frame.stage(1));
        submit_empty(frame.stage(0));
        lastPtr = &frame.stage(1);
    }

    wait(core, *lastPtr);
    EXPECT_GE(lastPtr->semaphore().gpuValue(), graph.firstFrameNumber() + FRAMES - 1);
}

JVK_TEST(StageGraph, skip_lets_the_waiter_go_on) {
    test::Context const context{};
    Core core{context.instance, test::Context::CONFIG, {}};
    StageGraph graph{
        core,
        {.stages = {{.queueIdx = 0}, {.queueIdx = 0, .waits = {TRANSFER_WAIT_ON_0}}}}
    };

    auto& frame = next_frame(core, graph);
    submit_empty(frame.stage(1));
    frame.stage(0).skip();

    EXPECT_EQ(frame.stage(0).status(), Stage::Status::SKIPPED);
    wait(core, frame.stage(1));
    wait(core, frame.stage(0));
    EXPECT_GE(frame.stage(0).semaphore().gpuValue(), frame.number());
}

JVK_TEST(StageGraph, next_frame_skips_a_forgotten_stage_of_the_frame_before) {
    test::Context const context{};
    Core core{context.instance, test::Context::CONFIG, {}};
    StageGraph graph{
        core,
        {
            .stages = {{.queueIdx = 0}, {.queueIdx = 0, .waits = {TRANSFER_WAIT_ON_0}}},
            .frameSlots = 2
        },
    };

    // the writer of the first frame gets no submit or skip, its waiter waits
    auto& first = next_frame(core, graph);
    submit_empty(first.stage(1));

    // the next frame skips it, not the reuse of its slot
    auto& second = next_frame(core, graph);
    ASSERT_EQ(first.stage(0).status(), Stage::Status::SKIPPED);

    // its waiter and the writer of the next frame go on before the frame after
    submit_empty(second.stage(0));
    wait(core, first.stage(1));
    wait(core, second.stage(0));
    EXPECT_GE(first.stage(0).semaphore().gpuValue(), first.number());

    submit_empty(second.stage(1));
    wait(core, second.stage(1));
}

JVK_TEST(StageGraph, scoped_submit_submits_at_its_end) {
    test::Context const context{};
    Core core{context.instance, test::Context::CONFIG, {}};
    StageGraph graph{core, {.stages = {{.queueIdx = 0}}}};

    auto const staging = counting_staging(core);
    Buffer<uint32_t> const buffer{core, DEVICE_CONFIG, VALUES};
    Buffer<uint32_t> const readback{core, test::READBACK_CONFIG, VALUES};

    auto& stage = next_frame(core, graph).stage(0);
    {
        auto const primary = stage.begin();
        buffer.copy(*primary, staging);
        readback.copy(*primary, buffer);
        primary->use(readback, HOST_READ);
        EXPECT_EQ(stage.status(), Stage::Status::UNSUBMITTED);
    }
    EXPECT_EQ(stage.status(), Stage::Status::SUBMITTED);

    wait(core, stage);
    EXPECT_TRUE(counts(readback));
}

JVK_TEST(StageGraph, moved_scoped_submit_submits_once) {
    test::Context const context{};
    Core core{context.instance, test::Context::CONFIG, {}};
    StageGraph graph{core, {.stages = {{.queueIdx = 0}}}};

    auto& stage = next_frame(core, graph).stage(0);
    {
        // the moved-from scope ends last: a second submit fails its check
        auto primary = stage.begin();
        auto const moved = std::move(primary);
    }
    EXPECT_EQ(stage.status(), Stage::Status::SUBMITTED);
    wait(core, stage);
}

JVK_TEST(StageGraph, submit_ends_an_open_rendering) {
    test::Context const context{};
    Core core{context.instance, test::Context::CONFIG, {}};
    StageGraph graph{core, {.stages = {{.queueIdx = 0}}}};

    auto& stage = next_frame(core, graph).stage(0);
    auto const primary = stage.begin();
    auto const rendering = primary->beginRendering({.area = AREA});
    stage.submit();
    EXPECT_EQ(primary->rendering(), nullptr);
    EXPECT_FALSE(primary->recording());

    wait(core, stage);
}

JVK_TEST(StageGraph, skip_discards_a_begun_primary) {
    test::Context const context{};
    Core core{context.instance, test::Context::CONFIG, {}};
    StageGraph graph{core, {.stages = {{.queueIdx = 0}}, .frameSlots = 1}};

    auto& stage = next_frame(core, graph).stage(0);
    {
        auto const primary = stage.begin();
        auto const rendering = primary->beginRendering({.area = AREA});
        stage.skip();
        EXPECT_EQ(stage.status(), Stage::Status::SKIPPED);
        EXPECT_EQ(primary->rendering(), nullptr);
        EXPECT_FALSE(primary->recording());
    }
    EXPECT_EQ(stage.status(), Stage::Status::SKIPPED);
    wait(core, stage);

    // the next frame of the slot begins the primary again
    auto& next = next_frame(core, graph).stage(0);
    submit_empty(next);
    wait(core, next);
}

JVK_TEST(StageGraph, exception_in_scoped_submit_submits_nothing) {
    test::Context const context{};
    Core core{context.instance, test::Context::CONFIG, {}};
    StageGraph graph{core, {.stages = {{.queueIdx = 0}}, .frameSlots = 2}};

    auto& stage = next_frame(core, graph).stage(0);
    try {
        auto const primary = stage.begin();
        auto const rendering = primary->beginRendering({.area = AREA});
        throw test_exception{};
    }
    catch(test_exception const&) {}

    EXPECT_EQ(stage.status(), Stage::Status::UNSUBMITTED);
    // the scoped_rendering ends the rendering on both paths
    EXPECT_EQ(stage.primary().rendering(), nullptr);
    EXPECT_TRUE(stage.primary().recording());

    // the next frame skips the stage
    auto& next = next_frame(core, graph).stage(0);
    EXPECT_EQ(stage.status(), Stage::Status::SKIPPED);
    submit_empty(next);
    wait(core, next);
    wait(core, stage);
}

JVK_TEST(StageGraph, destruction_after_an_exception_skips_the_unsubmitted_stage) {
    test::Context const context{};
    Core core{context.instance, test::Context::CONFIG, {}};

    // the waiter is submitted, ~StageGraph() skips the waited stage, else its job hangs
    auto const run = [&core] {
        StageGraph graph{
            core,
            {.stages = {{.queueIdx = 0}, {.queueIdx = 0, .waits = {TRANSFER_WAIT_ON_0}}}}
        };
        auto& frame = next_frame(core, graph);
        submit_empty(frame.stage(1));
        auto const primary = frame.stage(0).begin();
        throw test_exception{};
    };
    EXPECT_THROW(run(), test_exception);
}

JVK_TEST(StageGraph, submitted_and_co_await_of_a_stage) {
    test::Context const context{};
    Core core{context.instance, test::Context::CONFIG, {}};
    StageGraph graph{core, {.stages = {{.queueIdx = 0}}}};

    auto& frame = next_frame(core, graph);
    auto& stage = frame.stage(0);
    submit_empty(stage);

    auto const awaited = [](Stage const& s) -> cth::co::executor_task<bool> {
        co_await s.submitted();
        bool const marked = s.semaphore().submittedValue() >= s.frameNumber();
        co_await s;
        co_return marked && s.semaphore().gpuValue() >= s.frameNumber();
    };
    EXPECT_TRUE(core.executor().sync_wait(awaited(stage)));
}

JVK_TEST(StageGraph, extra_jvk_wait_and_binary_wait) {
    test::Context const context{};
    Core core{context.instance, test::Context::CONFIG, {}};
    StageGraph graph{core, {.stages = {{.queueIdx = 0}}}};
    Semaphore const external{core, cvk::create};
    cvk::Semaphore const binary{core, cvk::create};
    auto const& queue = core.queue(0);

    // the signal of the binary semaphore comes first
    core.executor().sync_wait(submit(queue, {.signals = {binary}}));

    auto& frame = next_frame(core, graph);
    auto& stage = frame.stage(0);
    stage.await(external, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, 1);
    stage.await(binary);
    submit_empty(stage);

    // the job waits for the mark of the external wait before its submit
    core.executor().sync_wait(
        submit(queue, {.signals = {SemaphoreSubmitInfo{.semaphore = external, .value = 1}}})
    );
    external.markSubmitted(1);

    wait(core, stage);
    EXPECT_EQ(external.gpuValue(), 1);
}

JVK_TEST(StageGraph, secondaries_record_on_several_threads) {
    constexpr size_t SECONDARIES = 4;
    test::Context const context{};
    Core core{context.instance, test::Context::CONFIG, {}};
    StageGraph graph{core, {.stages = {{.queueIdx = 0, .secondaries = SECONDARIES}}}};

    auto const staging = counting_staging(core);
    std::vector<Buffer<uint32_t>> readbacks{};
    for(size_t i = 0; i < SECONDARIES; ++i)
        readbacks.emplace_back(core, test::READBACK_CONFIG, VALUES);

    auto& frame = next_frame(core, graph);
    auto& stage = frame.stage(0);

    {
        std::vector<std::jthread> threads{};
        for(size_t i = 0; i < SECONDARIES; ++i)
            threads.emplace_back(
                [&stage, &readbacks, &staging, i] {
                    auto& secondary = stage.beginSecondary(i);
                    readbacks[i].copy(secondary, staging);
                }
            );
    }

    auto const primary = stage.begin();
    primary->execute(
        {stage.secondary(0), stage.secondary(1), stage.secondary(2), stage.secondary(3)}
    );
    for(auto const& readback : readbacks)
        primary->use(readback, HOST_READ);
    stage.submit();

    wait(core, stage);
    for(auto const& readback : readbacks)
        EXPECT_TRUE(counts(readback));
}

JVK_TEST(StageGraph, retain_lives_until_the_gpu_finished_the_submit) {
    test::Context const context{};
    Core core{context.instance, test::Context::CONFIG, {}};
    StageGraph graph{core, {.stages = {{.queueIdx = 0}}}};
    Semaphore const gate{core, cvk::create};

    auto const staging = counting_staging(core);
    Buffer<uint32_t> buffer{core, DEVICE_CONFIG, VALUES};
    auto const before = allocations(core);

    auto& stage = next_frame(core, graph).stage(0);
    // the gpu waits for the gate
    stage.await(gate, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, 1);
    auto const primary = stage.begin();
    buffer.copy(*primary, staging);
    stage.retain(std::move(buffer));
    stage.submit();

    gate.markSubmitted(1);
    core.executor().sync_wait(await_submitted(stage));
    EXPECT_EQ(allocations(core), before);

    host_signal(gate, 1);
    wait(core, stage);
    EXPECT_TRUE(reaches_allocations(core, before - 1));
}

JVK_TEST(StageGraph, retain_of_the_next_frame_survives_the_job_of_the_frame_before) {
    test::Context const context{};
    Core core{context.instance, test::Context::CONFIG, {}};
    StageGraph graph{core, {.stages = {{.queueIdx = 0}}, .frameSlots = 1}};
    Semaphore const gate{core, cvk::create};
    auto const& queue = core.queue(0);

    Buffer<uint32_t> first{core, DEVICE_CONFIG, VALUES};
    Buffer<uint32_t> second{core, DEVICE_CONFIG, VALUES};
    auto const before = allocations(core);

    auto& stage = next_frame(core, graph).stage(0);
    stage.await(gate, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, 1);
    stage.retain(std::move(first));
    submit_empty(stage);
    gate.markSubmitted(1);
    core.executor().sync_wait(await_submitted(stage));

    // the job waits for the gpu: its resume comes behind this strand job
    std::binary_semaphore blocking{0};
    std::binary_semaphore unblock{0};
    queue.executor().context().post(
        [&blocking, &unblock] {
            blocking.release();
            unblock.acquire();
        }
    );
    ASSERT_TRUE(blocking.try_acquire_for(TIMEOUT));
    host_signal(gate, 1);

    // the same slot: the gpu finished the frame before, its job did not destroy the retained objects yet
    auto& next = next_frame(core, graph).stage(0);
    next.retain(std::move(second));
    unblock.release();

    // the job of the frame before destroys its own retained buffer only
    EXPECT_TRUE(reaches_allocations(core, before - 1));
    core.executor().sync_wait(enter_strand(queue));
    EXPECT_EQ(allocations(core), before - 1);

    submit_empty(next);
    wait(core, next);
    EXPECT_TRUE(reaches_allocations(core, before - 2));
}

JVK_TEST(StageGraph, skip_destroys_the_retained_objects_after_its_signal) {
    test::Context const context{};
    Core core{context.instance, test::Context::CONFIG, {}};
    StageGraph graph{core, {.stages = {{.queueIdx = 0}}}};

    Buffer<uint32_t> buffer{core, DEVICE_CONFIG, VALUES};
    auto const before = allocations(core);

    auto& stage = next_frame(core, graph).stage(0);
    stage.retain(std::move(buffer));
    stage.skip();

    wait(core, stage);
    EXPECT_GE(stage.semaphore().gpuValue(), stage.frameNumber());
    EXPECT_TRUE(reaches_allocations(core, before - 1));
}

JVK_TEST(StageGraph, prologue_transitions_an_unknown_image_from_undefined) {
    test::Context const context{};
    Core core{context.instance, test::Context::CONFIG, {}};
    StageGraph graph{core, {.stages = {{.queueIdx = 0}}}};

    auto const staging = counting_staging(core);
    Image const image{core, IMAGE_CONFIG, EXTENT};
    Buffer<uint32_t> const readback{core, test::READBACK_CONFIG, VALUES};

    // no assume(): the first use needs the transition, the validation layers report a wrong layout
    auto& stage = next_frame(core, graph).stage(0);
    auto const primary = stage.begin();
    image.copy(*primary, staging);
    image.copyTo(*primary, readback);
    primary->use(readback, HOST_READ);
    stage.submit();

    wait(core, stage);
    EXPECT_TRUE(counts(readback));
}

JVK_TEST(StageGraph, retained_transient_image_starts_from_undefined) {
    test::Context const context{};
    Core core{context.instance, test::Context::CONFIG, {}};
    StageGraph graph{core, {.stages = {{.queueIdx = 0}}}};
    Semaphore const gate{core, cvk::create};

    auto const staging = counting_staging(core);
    Buffer<uint32_t> const readback{core, test::READBACK_CONFIG, VALUES};
    Image image{core, IMAGE_CONFIG, EXTENT};
    auto const before = allocations(core);

    auto& stage = next_frame(core, graph).stage(0);
    stage.await(gate, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, 1);
    auto const primary = stage.begin();
    image.copy(*primary, staging);
    image.copyTo(*primary, readback);
    primary->use(readback, HOST_READ);
    // the last use: the job records the transition from undefined after the release
    stage.retain(std::move(image));
    stage.submit();

    gate.markSubmitted(1);
    core.executor().sync_wait(await_submitted(stage));
    EXPECT_EQ(allocations(core), before);

    host_signal(gate, 1);
    wait(core, stage);
    EXPECT_TRUE(counts(readback));
    EXPECT_TRUE(reaches_allocations(core, before - 1));
}

JVK_TEST(StageGraph, image_moved_after_the_submit_keeps_its_prologue) {
    test::Context const context{};
    Core core{context.instance, test::Context::CONFIG, {}};
    StageGraph graph{core, {.stages = {{.queueIdx = 0}}}};
    Semaphore const gate{core, cvk::create};

    auto const staging = counting_staging(core);
    Buffer<uint32_t> const readback{core, test::READBACK_CONFIG, VALUES};
    std::optional<Image> firstOpt{std::in_place, core, IMAGE_CONFIG, EXTENT};
    std::optional<Image> secondOpt{};

    // the job awaits the mark of the gate before its append
    auto& stage = next_frame(core, graph).stage(0);
    stage.await(gate, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, 1);
    auto const primary = stage.begin();
    firstOpt->copy(*primary, staging);
    firstOpt->copyTo(*primary, readback);
    primary->use(readback, HOST_READ);
    stage.submit();

    // the recorded object is gone before the job records the prologue
    secondOpt.emplace(std::move(*firstOpt));
    firstOpt.reset();

    gate.markSubmitted(1);
    host_signal(gate, 1);
    wait(core, stage);
    EXPECT_TRUE(counts(readback));
}

JVK_TEST(StageGraph, image_and_buffer_move_to_another_queue_family) {
    test::Context const context{};
    Core core{context.instance, two_families(), {}};
    if(core.queue(0).vk().familyIndex() == core.queue(1).vk().familyIndex())
        GTEST_SKIP() << "needs two queue families, e.g. ANV_QUEUE_OVERRIDE=gc=2,c=1";
    StageGraph graph{
        core,
        {.stages = {{.queueIdx = 0}, {.queueIdx = 1, .waits = {TRANSFER_WAIT_ON_0}}}}
    };

    auto const staging = counting_staging(core);
    Image const image{core, IMAGE_CONFIG, EXTENT};
    Buffer<uint32_t> const buffer{core, DEVICE_CONFIG, VALUES};
    Buffer<uint32_t> const imageReadback{core, test::READBACK_CONFIG, VALUES};
    Buffer<uint32_t> const bufferReadback{core, test::READBACK_CONFIG, VALUES};

    auto& frame = next_frame(core, graph);
    auto& writer = frame.stage(0);
    auto& reader = frame.stage(1);

    // the reader submits first: its job appends after the writer, the writer queue releases both
    auto const readerPrimary = reader.begin();
    image.copyTo(*readerPrimary, imageReadback);
    bufferReadback.copy(*readerPrimary, buffer);
    readerPrimary->use(imageReadback, HOST_READ, bufferReadback, HOST_READ);
    reader.submit();

    auto const writerPrimary = writer.begin();
    image.copy(*writerPrimary, staging);
    buffer.copy(*writerPrimary, staging);
    writer.submit();

    wait(core, reader);
    EXPECT_TRUE(counts(imageReadback));
    EXPECT_TRUE(counts(bufferReadback));

    // one release on the writer queue for both: the next release gets the second value of its counter
    auto const& writerQueue = core.queue(0);
    auto const next = core.executor().sync_wait(writerQueue.release(cvk::Barrier{}));
    EXPECT_EQ(next.value, 2);
    writerQueue.waitIdle();
}

JVK_TEST(StageGraph, erase_removes_the_state_and_the_last_queue_of_an_image) {
    test::Context const context{};
    Core core{context.instance, two_families(), {}};
    auto const& firstQueue = core.queue(0);
    if(firstQueue.vk().familyIndex() == core.queue(1).vk().familyIndex())
        GTEST_SKIP() << "needs two queue families, e.g. ANV_QUEUE_OVERRIDE=gc=2,c=1";
    StageGraph graph{core, {.stages = {{.queueIdx = 0}, {.queueIdx = 1}}}};

    // one staging buffer per family: a shared one would need a release too
    auto const firstStaging = counting_staging(core);
    auto const secondStaging = counting_staging(core);
    Image const image{core, IMAGE_CONFIG, EXTENT};
    Buffer<uint32_t> const readback{core, test::READBACK_CONFIG, VALUES};

    auto& first = next_frame(core, graph);
    auto const firstPrimary = first.stage(0).begin();
    image.copy(*firstPrimary, firstStaging);
    first.stage(0).submit();
    first.stage(1).skip();
    wait(core, first.stage(0));

    graph.erase(image);

    // the graph does not know the image: the other family starts from undefined, no release on the first queue
    auto& second = next_frame(core, graph);
    second.stage(0).skip();
    auto const primary = second.stage(1).begin();
    image.copy(*primary, secondStaging);
    image.copyTo(*primary, readback);
    primary->use(readback, HOST_READ);
    second.stage(1).submit();

    wait(core, second.stage(1));
    EXPECT_TRUE(counts(readback));

    auto const next = core.executor().sync_wait(firstQueue.release(cvk::Barrier{}));
    EXPECT_EQ(next.value, 1);
    firstQueue.waitIdle();
}
}
