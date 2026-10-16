#include "context.hpp"
#include "test.hpp"

#include "cvk/render/sync/fence.hpp"


namespace cvk {
namespace {
    VkSubmitInfo2 const EMPTY_SUBMIT{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .pNext = nullptr,
        .flags = 0,
        .waitSemaphoreInfoCount = 0,
        .pWaitSemaphoreInfos = nullptr,
        .commandBufferInfoCount = 0,
        .pCommandBufferInfos = nullptr,
        .signalSemaphoreInfoCount = 0,
        .pSignalSemaphoreInfos = nullptr,
    };
}

CVK_TEST(Fence, base_constructor_does_not_create) {
    test::Context const context{};

    Fence const fence{context.core};

    EXPECT_FALSE(fence.created());
}

CVK_TEST(Fence, create_not_signaled) {
    test::Context const context{};

    Fence const fence{context.core, 0};

    ASSERT_TRUE(fence.created());
    EXPECT_EQ(fence.status(), VK_NOT_READY);
    EXPECT_FALSE(fence.signaled());
    EXPECT_EQ(fence.wait(0), VK_TIMEOUT);
}

CVK_TEST(Fence, create_signaled_and_reset) {
    test::Context const context{};

    Fence const fence{context.core, VK_FENCE_CREATE_SIGNALED_BIT};

    ASSERT_TRUE(fence.signaled());
    fence.wait();

    fence.reset();
    EXPECT_FALSE(fence.signaled());
}

CVK_TEST(Fence, submit_signals) {
    test::Context const context{};
    Fence const fence{context.core, 0};

    context.core.queue(0).raw_submit(EMPTY_SUBMIT, fence.get());

    fence.wait();
    EXPECT_TRUE(fence.signaled());

    fence.waitReset();
    EXPECT_FALSE(fence.signaled());
}

CVK_TEST(Fence, wait_reset_timeout_keeps_the_state) {
    test::Context const context{};
    Fence const fence{context.core, 0};

    EXPECT_EQ(fence.waitReset(0), VK_TIMEOUT);
    EXPECT_FALSE(fence.signaled());

    context.core.queue(0).raw_submit(EMPTY_SUBMIT, fence.get());

    EXPECT_EQ(fence.waitReset(UINT64_MAX), VK_SUCCESS);
    EXPECT_FALSE(fence.signaled());
}

CVK_TEST(Fence, release_and_wrap) {
    test::Context const context{};
    Fence fence{context.core, VK_FENCE_CREATE_SIGNALED_BIT};
    auto const vkFence = fence.get();

    auto const state = fence.release();
    EXPECT_FALSE(fence.created());
    EXPECT_EQ(state.vkFence.get(), vkFence);

    Fence const wrapped{context.core, state};
    ASSERT_TRUE(wrapped.created());
    EXPECT_EQ(wrapped.get(), vkFence);
    EXPECT_TRUE(wrapped.signaled());
}

CVK_TEST(Fence, move_assignment_destroys_the_old_fence) {
    test::Context const context{};
    Fence fence{context.core, 0};
    Fence other{context.core, VK_FENCE_CREATE_SIGNALED_BIT};
    auto const vkFence = other.get();

    fence = std::move(other);

    EXPECT_EQ(fence.get(), vkFence);
    EXPECT_FALSE(other.created());
    EXPECT_TRUE(fence.signaled());
}
}
