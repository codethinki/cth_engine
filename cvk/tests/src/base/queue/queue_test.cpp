#include "context.hpp"
#include "headless.hpp"
#include "test.hpp"

#include "cvk/base/device.hpp"
#include "cvk/base/instance.hpp"
#include "cvk/base/physical_device.hpp"

#include <cstdint>


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

CVK_TEST(QueueFamily, graphics_or_compute_supports_transfer) {
    using enum QueueFamilyProperties;
    static_assert(to_queue_properties(VK_QUEUE_GRAPHICS_BIT, false) == (GRAPHICS | TRANSFER));
    static_assert(to_queue_properties(VK_QUEUE_COMPUTE_BIT, false) == (COMPUTE | TRANSFER));
    static_assert(to_queue_properties(VK_QUEUE_TRANSFER_BIT, true) == (TRANSFER | PRESENT));
    static_assert(to_queue_properties(VK_QUEUE_SPARSE_BINDING_BIT, false) == NONE);
}

CVK_TEST(Queue, copies_share_the_queue) {
    test::Context const context{};
    auto const& queue = context.core.queue(0);

    Queue const copy{queue};

    EXPECT_EQ(copy.get(), queue.get());
    EXPECT_EQ(copy.familyIndex(), queue.familyIndex());
    EXPECT_EQ(copy.index(), queue.index());
    copy.waitIdle();
}

CVK_TEST(Queue, raw_submit_signals_the_fence) {
    test::Context const context{};
    auto const table = context.core.deviceTable();

    VkFenceCreateInfo const fenceInfo{.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .pNext = nullptr, .flags = 0};
    VkFence fence = VK_NULL_HANDLE;
    ASSERT_EQ(table->vkCreateFence(table.device(), &fenceInfo, nullptr, &fence), VK_SUCCESS);

    context.core.queue(0).rawSubmit(EMPTY_SUBMIT, fence);

    EXPECT_EQ(table->vkWaitForFences(table.device(), 1, &fence, VK_TRUE, UINT64_MAX), VK_SUCCESS);

    table->vkDestroyFence(table.device(), fence, nullptr);
}

CVK_TEST(Queue, raw_submit_without_fence) {
    test::Context const context{};
    auto const& queue = context.core.queue(0);

    queue.rawSubmit(EMPTY_SUBMIT);
    queue.waitIdle();
}

CVK_TEST(Queue, copies_keep_the_queue_through_a_device_release_and_wrap) {
    test::Context const context{};
    auto const [physicalDevice, familyIndices] = test::pick(context.instance, test::QUEUES);
    Device device{context.instance, physicalDevice, Device::Config{familyIndices}};

    Queue const copy{device.queues()[0]};
    Device const wrapped{context.instance, physicalDevice, device.release()};

    ASSERT_EQ(wrapped.queues().size(), 1);
    EXPECT_EQ(wrapped.queues()[0].get(), copy.get());
    copy.waitIdle();
}

CVK_TEST(Queue, present_family_can_present) {
    test::PresentContext const context{};

    EXPECT_TRUE(context.queue().canPresent());
}
}
