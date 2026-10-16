#include "context.hpp"
#include "test.hpp"

#include "cvk/render/sync/fence.hpp"
#include "cvk/render/sync/semaphore.hpp"


namespace cvk {
namespace {
    VkSemaphoreSubmitInfo semaphore_info(Semaphore const& semaphore) {
        return VkSemaphoreSubmitInfo{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .pNext = nullptr,
            .semaphore = semaphore.get(),
            .value = 0,
            .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
            .deviceIndex = 0,
        };
    }
}

CVK_TEST(Semaphore, base_constructor_does_not_create) {
    test::Context const context{};

    Semaphore const semaphore{context.core};

    EXPECT_FALSE(semaphore.created());
}

CVK_TEST(Semaphore, create_and_destroy) {
    test::Context const context{};
    Semaphore semaphore{context.core, create};

    ASSERT_TRUE(semaphore.created());
    EXPECT_EQ(semaphore.deviceTable().device(), context.core.vkDevice());

    semaphore.destroy();
    EXPECT_FALSE(semaphore.created());
}

CVK_TEST(Semaphore, submit_signals_and_waits) {
    test::Context const context{};
    Semaphore const semaphore{context.core, create};
    Fence const fence{context.core, 0};
    auto const& queue = context.core.queue(0);

    auto const info = semaphore_info(semaphore);

    VkSubmitInfo2 const signalSubmit{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .pNext = nullptr,
        .flags = 0,
        .waitSemaphoreInfoCount = 0,
        .pWaitSemaphoreInfos = nullptr,
        .commandBufferInfoCount = 0,
        .pCommandBufferInfos = nullptr,
        .signalSemaphoreInfoCount = 1,
        .pSignalSemaphoreInfos = &info,
    };
    VkSubmitInfo2 const waitSubmit{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .pNext = nullptr,
        .flags = 0,
        .waitSemaphoreInfoCount = 1,
        .pWaitSemaphoreInfos = &info,
        .commandBufferInfoCount = 0,
        .pCommandBufferInfos = nullptr,
        .signalSemaphoreInfoCount = 0,
        .pSignalSemaphoreInfos = nullptr,
    };

    queue.raw_submit(signalSubmit);
    queue.raw_submit(waitSubmit, fence.get());

    fence.wait();
}

CVK_TEST(Semaphore, release_and_wrap) {
    test::Context const context{};
    Semaphore semaphore{context.core, create};
    auto const vkSemaphore = semaphore.get();

    auto const state = semaphore.release();
    EXPECT_FALSE(semaphore.created());

    Semaphore const wrapped{context.core, state};
    EXPECT_EQ(wrapped.get(), vkSemaphore);
}

CVK_TEST(Semaphore, move_assignment_destroys_the_old_semaphore) {
    test::Context const context{};
    Semaphore semaphore{context.core, create};
    Semaphore other{context.core, create};
    auto const vkSemaphore = other.get();

    semaphore = std::move(other);

    EXPECT_EQ(semaphore.get(), vkSemaphore);
    EXPECT_FALSE(other.created());
}
}
