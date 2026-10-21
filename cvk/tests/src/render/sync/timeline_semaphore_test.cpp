#include "context.hpp"
#include "test.hpp"

#include "cvk/render/sync/timeline_semaphore.hpp"

#include <concepts>


namespace cvk {
CVK_TEST(TimelineSemaphore, base_constructor_does_not_create) {
    test::Context const context{};

    TimelineSemaphore const semaphore{context.core};

    EXPECT_FALSE(semaphore.created());
}

CVK_TEST(TimelineSemaphore, create_sets_the_initial_value) {
    test::Context const context{};
    TimelineSemaphore semaphore{context.core};

    semaphore.create(5);

    ASSERT_TRUE(semaphore.created());
    EXPECT_EQ(semaphore.value(), 5);
    EXPECT_EQ(semaphore.gpuValue(), 5);
    EXPECT_EQ(semaphore.wait(semaphore.value(), 0), VK_SUCCESS);
}

CVK_TEST(TimelineSemaphore, signal_advances_the_counter) {
    test::Context const context{};
    TimelineSemaphore semaphore{context.core, create};

    EXPECT_EQ(semaphore.signal(), 1);
    EXPECT_EQ(semaphore.value(), 1);
    EXPECT_EQ(semaphore.gpuValue(), 1);
    semaphore.wait();
}

CVK_TEST(TimelineSemaphore, wait_for_a_value_not_signaled_times_out) {
    test::Context const context{};
    TimelineSemaphore const semaphore{context.core, create};

    EXPECT_EQ(semaphore.next(), 1);

    EXPECT_EQ(semaphore.wait(semaphore.value(), 0), VK_TIMEOUT);
    EXPECT_EQ(semaphore.gpuValue(), 0);
}

CVK_TEST(TimelineSemaphore, wait_for_an_older_value) {
    test::Context const context{};
    TimelineSemaphore semaphore{context.core, create};

    EXPECT_EQ(semaphore.signal(), 1);
    EXPECT_EQ(semaphore.next(), 2);

    // the counter is ahead of the semaphore, an older value is reached
    EXPECT_EQ(semaphore.wait(1, 0), VK_SUCCESS);
    EXPECT_EQ(semaphore.wait(2, 0), VK_TIMEOUT);
    semaphore.wait(1);
}

CVK_TEST(TimelineSemaphore, submit_signals_the_next_value) {
    test::Context const context{};
    TimelineSemaphore const semaphore{context.core, create};

    VkSemaphoreSubmitInfo const signalInfo{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .pNext = nullptr,
        .semaphore = semaphore.get(),
        .value = semaphore.next(),
        .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        .deviceIndex = 0,
    };
    VkSubmitInfo2 const submit{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .pNext = nullptr,
        .flags = 0,
        .waitSemaphoreInfoCount = 0,
        .pWaitSemaphoreInfos = nullptr,
        .commandBufferInfoCount = 0,
        .pCommandBufferInfos = nullptr,
        .signalSemaphoreInfoCount = 1,
        .pSignalSemaphoreInfos = &signalInfo,
    };

    context.core.queue(0).rawSubmit(submit);

    semaphore.wait();
    EXPECT_EQ(semaphore.gpuValue(), signalInfo.value);
}

CVK_TEST(TimelineSemaphore, release_and_wrap_keep_the_counter) {
    test::Context const context{};
    TimelineSemaphore semaphore{context.core, create};
    [[maybe_unused]] auto const signaled = semaphore.signal();

    auto const state = semaphore.release();
    EXPECT_FALSE(semaphore.created());
    EXPECT_EQ(semaphore.value(), 0);
    EXPECT_EQ(state.value, 1);

    TimelineSemaphore const wrapped{context.core, state};
    EXPECT_EQ(wrapped.get(), state.vkSemaphore.get());
    EXPECT_EQ(wrapped.value(), 1);
    EXPECT_EQ(wrapped.gpuValue(), 1);
}

CVK_TEST(TimelineSemaphore, move_keeps_the_counter) {
    test::Context const context{};
    TimelineSemaphore semaphore{context.core, create};
    semaphore.create(3);

    TimelineSemaphore const moved{std::move(semaphore)};

    EXPECT_FALSE(semaphore.created());
    EXPECT_EQ(moved.value(), 3);
    EXPECT_EQ(moved.gpuValue(), 3);
}

CVK_TEST(TimelineSemaphore, move_assignment_destroys_the_old_semaphore) {
    test::Context const context{};
    TimelineSemaphore semaphore{context.core, create};
    TimelineSemaphore other{context.core, create};
    other.create(7);
    auto const vkSemaphore = other.get();

    semaphore = std::move(other);

    EXPECT_EQ(semaphore.get(), vkSemaphore);
    EXPECT_EQ(semaphore.value(), 7);
    EXPECT_FALSE(other.created());
}

CVK_TEST(TimelineSemaphore, is_not_a_binary_semaphore) {
    static_assert(!std::convertible_to<TimelineSemaphore const*, Semaphore const*>);
    static_assert(!std::convertible_to<TimelineSemaphore const&, Semaphore const&>);
}
}
