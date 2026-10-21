#include "headless.hpp"
#include "test.hpp"

#include "cvk/base/device.hpp"
#include "cvk/base/instance.hpp"
#include "cvk/base/physical_device.hpp"
#include "cvk/utility/constant/device_constants.hpp"

#include <array>
#include <cstdint>
#include <utility>
#include <vector>


namespace cvk {
namespace {
    constexpr std::array QUEUES{QueueFamilyProperties::GRAPHICS};

    /**
     * @brief device config with the base features, vkQueueSubmit2() needs synchronization2
     */
    Device::Config device_config(std::vector<queue_family_index_t> familyIndices) {
        return Device::Config{
            .queueFamilyIndices = std::move(familyIndices),
            .features = constants::BASE_DEVICE_FEATURES,
        };
    }

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

CVK_TEST(Queue, copies_share_the_queue) {
    Instance const instance{test::instance_config(), create};
    auto const [physicalDevice, familyIndices] = test::pick(instance, QUEUES);
    Device const device{instance, physicalDevice, device_config(familyIndices)};

    auto const& queue = device.queues()[0];
    Queue const copy{queue};

    EXPECT_EQ(copy.get(), queue.get());
    EXPECT_EQ(copy.familyIndex(), queue.familyIndex());
    EXPECT_EQ(copy.index(), queue.index());
    copy.waitIdle();
}

CVK_TEST(Queue, raw_submit_signals_the_fence) {
    Instance const instance{test::instance_config(), create};
    auto const [physicalDevice, familyIndices] = test::pick(instance, QUEUES);
    Device const device{instance, physicalDevice, device_config(familyIndices)};
    auto const table = device.table();

    VkFenceCreateInfo const fenceInfo{.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .pNext = nullptr, .flags = 0};
    VkFence fence = VK_NULL_HANDLE;
    ASSERT_EQ(table->vkCreateFence(table.device(), &fenceInfo, nullptr, &fence), VK_SUCCESS);

    auto const& queue = device.queues()[0];
    queue.rawSubmit(EMPTY_SUBMIT, fence);

    EXPECT_EQ(table->vkWaitForFences(table.device(), 1, &fence, VK_TRUE, UINT64_MAX), VK_SUCCESS);

    table->vkDestroyFence(table.device(), fence, nullptr);
}

CVK_TEST(Queue, raw_submit_without_fence) {
    Instance const instance{test::instance_config(), create};
    auto const [physicalDevice, familyIndices] = test::pick(instance, QUEUES);
    Device const device{instance, physicalDevice, device_config(familyIndices)};

    auto const& queue = device.queues()[0];
    queue.rawSubmit(EMPTY_SUBMIT);
    queue.waitIdle();
}

CVK_TEST(Queue, copies_keep_the_queue_through_a_device_release_and_wrap) {
    Instance const instance{test::instance_config(), create};
    auto const [physicalDevice, familyIndices] = test::pick(instance, QUEUES);
    Device device{instance, physicalDevice, device_config(familyIndices)};

    Queue const copy{device.queues()[0]};
    Device const wrapped{instance, physicalDevice, device.release()};

    ASSERT_EQ(wrapped.queues().size(), 1);
    EXPECT_EQ(wrapped.queues()[0].get(), copy.get());
    copy.waitIdle();
}

CVK_TEST(Queue, present_family_can_present) {
    Instance const instance{test::instance_config(test::headless_extensions()), create};
    std::array const surfaces{test::headless_surface(instance)};
    std::array const queues{QueueFamilyProperties::GRAPHICS | QueueFamilyProperties::PRESENT};

    auto const [physicalDevice, familyIndices] = test::pick(instance, queues, surfaces);
    Device const device{instance, physicalDevice, device_config(familyIndices)};

    EXPECT_TRUE(device.queues()[0].canPresent());
}
}
