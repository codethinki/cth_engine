#include "headless.hpp"
#include "test.hpp"

#include "cvk/base/device.hpp"
#include "cvk/base/instance.hpp"
#include "cvk/base/physical_device.hpp"

#include <array>
#include <cstdint>


namespace cvk {
using test::APP_NAME;
using test::NO_EXTENSIONS;

namespace {
    constexpr std::array QUEUES{QueueFamilyProperties::GRAPHICS};

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

CVK_TEST(Queue, base_constructor_does_not_create) {
    Queue const queue{QueueFamilyProperties::GRAPHICS};

    EXPECT_FALSE(queue.created());
    EXPECT_FALSE(queue.can_present());
}

CVK_TEST(Queue, copies_share_the_queue) {
    Instance const instance{APP_NAME, NO_EXTENSIONS, std::nullopt};
    auto const [physicalDevice, familyIndices] = test::pick(instance, QUEUES);
    Device const device{instance, physicalDevice, Device::Config{familyIndices}};

    auto const& queue = device.queues()[0];
    Queue const copy{queue};

    ASSERT_TRUE(copy.created());
    EXPECT_EQ(copy.get(), queue.get());
    EXPECT_EQ(copy.familyIndex(), queue.familyIndex());
    EXPECT_EQ(copy.index(), queue.index());
    copy.wait();
}

CVK_TEST(Queue, raw_submit_signals_the_fence) {
    Instance const instance{APP_NAME, NO_EXTENSIONS, std::nullopt};
    auto const [physicalDevice, familyIndices] = test::pick(instance, QUEUES);
    Device const device{instance, physicalDevice, Device::Config{familyIndices}};
    auto const table = device.table();

    VkFenceCreateInfo const fenceInfo{.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .pNext = nullptr, .flags = 0};
    VkFence fence = VK_NULL_HANDLE;
    ASSERT_EQ(table->vkCreateFence(table.device(), &fenceInfo, nullptr, &fence), VK_SUCCESS);

    auto const& queue = device.queues()[0];
    queue.raw_submit(EMPTY_SUBMIT, fence);

    EXPECT_EQ(table->vkWaitForFences(table.device(), 1, &fence, VK_TRUE, UINT64_MAX), VK_SUCCESS);

    table->vkDestroyFence(table.device(), fence, nullptr);
}

CVK_TEST(Queue, raw_submit_without_fence) {
    Instance const instance{APP_NAME, NO_EXTENSIONS, std::nullopt};
    auto const [physicalDevice, familyIndices] = test::pick(instance, QUEUES);
    Device const device{instance, physicalDevice, Device::Config{familyIndices}};

    auto const& queue = device.queues()[0];
    queue.raw_submit(EMPTY_SUBMIT);
    queue.wait();
}

CVK_TEST(Queue, release_and_wrap) {
    Instance const instance{APP_NAME, NO_EXTENSIONS, std::nullopt};
    auto const [physicalDevice, familyIndices] = test::pick(instance, QUEUES);
    Device const device{instance, physicalDevice, Device::Config{familyIndices}};

    Queue copy{device.queues()[0]};
    auto const vkQueue = copy.get();

    auto state = copy.release();
    EXPECT_FALSE(copy.created());

    Queue const wrapped{copy.familyProperties(), std::move(state)};

    ASSERT_TRUE(wrapped.created());
    EXPECT_EQ(wrapped.get(), vkQueue);
    wrapped.wait();
}

CVK_TEST(Queue, present_family_can_present) {
    Instance const instance{APP_NAME, test::headless_extensions(), std::nullopt};
    std::array const surfaces{test::headless_surface(instance)};
    std::array const queues{QueueFamilyProperties::GRAPHICS | QueueFamilyProperties::PRESENT};

    auto const [physicalDevice, familyIndices] = test::pick(instance, queues, surfaces);
    Device const device{instance, physicalDevice, Device::Config{familyIndices}};

    EXPECT_TRUE(device.queues()[0].can_present());
}
}
