#include "context.hpp"
#include "test.hpp"

#include "cvk/base/queue/queue.hpp"
#include "cvk/render/cmd/cmd_buffer.hpp"
#include "cvk/render/cmd/cmd_pool.hpp"
#include "cvk/render/sync/fence.hpp"
#include "cvk/render/sync/semaphore.hpp"
#include "cvk/render/sync/timeline_semaphore.hpp"

#include <array>
#include <variant>


namespace cvk {
CVK_TEST(Submit, semaphore_info_takes_the_kind_from_the_semaphore) {
    test::Context const context{};
    Semaphore const binary{context.core, create};
    TimelineSemaphore const timeline{context.core, create};

    SemaphoreSubmitInfo const binaryInfo{binary};
    SemaphoreSubmitInfo const timelineInfo{timeline, VK_PIPELINE_STAGE_2_TRANSFER_BIT, 3};

    EXPECT_TRUE(std::holds_alternative<not_null_ptr<Semaphore const>>(binaryInfo.semaphore));
    EXPECT_EQ(binaryInfo.stages, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT);
    EXPECT_FALSE(binaryInfo.value.has_value());

    EXPECT_TRUE(std::holds_alternative<not_null_ptr<TimelineSemaphore const>>(timelineInfo.semaphore));
    EXPECT_EQ(timelineInfo.stages, VK_PIPELINE_STAGE_2_TRANSFER_BIT);
    EXPECT_EQ(timelineInfo.value, 3);
}

CVK_TEST(Submit, command_buffer_without_fence) {
    test::Context const context{};
    CmdPool const pool{context.core, CmdPool::Config{context.core.queue(0)}, create};
    PrimaryCmdBuffer primary{pool, create};
    primary.begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    primary.end();

    context.core.queue(0).submit({.cmdBuffers = {&primary}});

    context.core.queue(0).waitIdle();
}

CVK_TEST(Submit, binary_semaphore_orders_two_infos) {
    test::Context const context{};
    Semaphore const semaphore{context.core, create};
    Fence const fence{context.core, 0};

    std::array const infos{
        SubmitInfo{.signals = {semaphore}},
        SubmitInfo{.waits = {semaphore}},
    };
    context.core.queue(0).submit(infos, &fence);

    fence.wait();
}

CVK_TEST(Submit, timeline_signal_takes_the_next_value) {
    test::Context const context{};
    TimelineSemaphore const timeline{context.core, create};

    context.core.queue(0).submit({.signals = {timeline}});

    EXPECT_EQ(timeline.value(), 1);
    timeline.wait();
    EXPECT_EQ(timeline.gpuValue(), 1);
}

CVK_TEST(Submit, timeline_values_resolve_in_order) {
    test::Context const context{};
    TimelineSemaphore const first{context.core, create};
    TimelineSemaphore const second{context.core, create};

    // the wait of the second info resolves after the signal of the first: it waits for 1
    std::array const infos{
        SubmitInfo{.signals = {first}},
        SubmitInfo{.waits = {first}, .signals = {second}},
    };
    context.core.queue(0).submit(infos);

    second.wait();
    EXPECT_EQ(first.gpuValue(), 1);
    EXPECT_EQ(second.gpuValue(), 1);
}

CVK_TEST(Submit, explicit_timeline_value_keeps_the_counter) {
    test::Context const context{};
    TimelineSemaphore const timeline{context.core, create};

    context.core.queue(0).submit({.signals = {{timeline, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, 5}}});

    context.core.queue(0).waitIdle();
    EXPECT_EQ(timeline.gpuValue(), 5);
    EXPECT_EQ(timeline.value(), 0);
}
}
