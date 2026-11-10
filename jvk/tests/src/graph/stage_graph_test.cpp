#include "jvk/graph/stage_graph.hpp"

#include "context.hpp"
#include "test.hpp"

#include "jvk/base/queue.hpp"
#include "jvk/data/buffer.hpp"

#include "cvk/base/queue/submit_info.hpp"
#include "cvk/render/sync/semaphore.hpp"

#include <cth/coro/tasks/executor_task.hpp>

#include <cstddef>
#include <cstdint>
#include <numeric>
#include <thread>
#include <utility>
#include <vector>


namespace jvk {
namespace {
    using enum cvk::QueueFamilyProperties;

    constexpr size_t VALUES = 64;

    constexpr cvk::BaseBuffer::Config DEVICE_CONFIG{
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
    };

    constexpr BufferAccess HOST_READ{VK_PIPELINE_STAGE_2_HOST_BIT, VK_ACCESS_2_HOST_READ_BIT};

    constexpr StageWait TRANSFER_WAIT_ON_0{.stageId = 0, .stages = VK_PIPELINE_STAGE_2_TRANSFER_BIT};

    /**
     * two graphics queues, two vulkan queues where the device has them
     */
    Core::Config two_queues() {
        auto config = test::Context::CONFIG;
        config.vk.queueProperties = {GRAPHICS, GRAPHICS};
        return config;
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
            if(mapping[i] != i) return false;
        return true;
    }

    Frame& next_frame(Core const& core, StageGraph& graph) { return core.executor().sync_wait(graph.nextFrame()); }

    cth::co::executor_task<void> await_stage(Stage const& stage) { co_await stage; }

    /**
     * blocks until the gpu finished the stage
     */
    void wait(Core const& core, Stage const& stage) { core.executor().sync_wait(await_stage(stage)); }

    /**
     * submits an empty primary
     */
    void submit_empty(Stage& stage) {
        stage.beginPrimary();
        stage.submit();
    }

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
    StageGraph graph{core, {.stages = {{.queueIdx = 0}, {.queueIdx = 0, .waits = {TRANSFER_WAIT_ON_0}}}}};

    auto const staging = counting_staging(core);
    Buffer<uint32_t> const buffer{core, DEVICE_CONFIG, VALUES};
    Buffer<uint32_t> const readback{core, test::READBACK_CONFIG, VALUES};

    auto& frame = next_frame(core, graph);
    auto& writer = frame.stage(0);
    auto& reader = frame.stage(1);

    // the reader submits first, from another thread: its job waits for the mark of the writer
    std::jthread{[&] {
        auto& primary = reader.beginPrimary();
        readback.copy(primary, buffer);
        primary.use(readback, HOST_READ);
        reader.submit();
    }}.join();

    auto& primary = writer.beginPrimary();
    buffer.copy(primary, staging);
    writer.submit();

    wait(core, reader);
    EXPECT_TRUE(counts(readback));
}

JVK_TEST(StageGraph, chain_on_two_queues) {
    test::Context const context{};
    Core core{context.instance, two_queues(), {}};
    if(&core.queue(0) == &core.queue(1)) GTEST_SKIP() << "needs two vulkan queues, e.g. ANV_QUEUE_OVERRIDE=gc=2,c=1";
    StageGraph graph{core, {.stages = {{.queueIdx = 0}, {.queueIdx = 1, .waits = {TRANSFER_WAIT_ON_0}}}}};

    auto const staging = counting_staging(core);
    Buffer<uint32_t> const buffer{core, DEVICE_CONFIG, VALUES};
    Buffer<uint32_t> const readback{core, test::READBACK_CONFIG, VALUES};

    auto& frame = next_frame(core, graph);
    auto& writer = frame.stage(0);
    auto& reader = frame.stage(1);

    // another queue: the reader submits at once, the gpu waits
    auto& readerPrimary = reader.beginPrimary();
    readback.copy(readerPrimary, buffer);
    readerPrimary.use(readback, HOST_READ);
    reader.submit();

    auto& writerPrimary = writer.beginPrimary();
    buffer.copy(writerPrimary, staging);
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
        {.stages = {{.queueIdx = 0}, {.queueIdx = 0, .waits = {{.stageId = 0, .frameOffset = 1}}}}, .frameSlots = 2},
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
    StageGraph graph{core, {.stages = {{.queueIdx = 0}, {.queueIdx = 0, .waits = {TRANSFER_WAIT_ON_0}}}}};

    auto& frame = next_frame(core, graph);
    submit_empty(frame.stage(1));
    frame.stage(0).skip();

    EXPECT_EQ(frame.stage(0).status(), Stage::Status::SKIPPED);
    wait(core, frame.stage(1));
    wait(core, frame.stage(0));
    EXPECT_GE(frame.stage(0).semaphore().gpuValue(), frame.number());
}

JVK_TEST(StageGraph, next_frame_skips_a_forgotten_stage) {
    test::Context const context{};
    Core core{context.instance, test::Context::CONFIG, {}};
    StageGraph graph{
        core,
        {.stages = {{.queueIdx = 0}, {.queueIdx = 0, .waits = {TRANSFER_WAIT_ON_0}}}, .frameSlots = 2},
    };

    // the writer of the first frame gets no submit or skip, its waiter waits
    auto& first = next_frame(core, graph);
    auto const firstNumber = first.number();
    auto const& forgottenSemaphore = first.stage(0).semaphore();
    auto const& waiterSemaphore = first.stage(1).semaphore();
    submit_empty(first.stage(1));

    auto& second = next_frame(core, graph);
    submit_empty(second.stage(0));
    submit_empty(second.stage(1));

    // the slot of the first frame comes back: the forgotten stage is skipped, its waiter goes on
    auto& third = next_frame(core, graph);
    EXPECT_GE(forgottenSemaphore.gpuValue(), firstNumber);
    EXPECT_GE(waiterSemaphore.gpuValue(), firstNumber);

    submit_empty(third.stage(0));
    submit_empty(third.stage(1));
    wait(core, third.stage(1));
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
    core.executor().sync_wait(submit(queue, {.signals = {SemaphoreSubmitInfo{.semaphore = external, .value = 1}}}));
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
            threads.emplace_back([&stage, &readbacks, &staging, i] {
                auto& secondary = stage.beginSecondary(i);
                readbacks[i].copy(secondary, staging);
            });
    }

    auto& primary = stage.beginPrimary();
    primary.execute({stage.secondary(0), stage.secondary(1), stage.secondary(2), stage.secondary(3)});
    for(auto const& readback : readbacks)
        primary.use(readback, HOST_READ);
    stage.submit();

    wait(core, stage);
    for(auto const& readback : readbacks)
        EXPECT_TRUE(counts(readback));
}
}
