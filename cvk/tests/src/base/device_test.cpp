#include "headless.hpp"
#include "test.hpp"
#include "validation.hpp"

#include "cvk/base/device.hpp"
#include "cvk/base/instance.hpp"
#include "cvk/base/physical_device.hpp"
#include "cvk/utility/constant/device_constants.hpp"
#include "cvk/utility/constants.hpp"

#include <array>
#include <map>
#include <ranges>
#include <string>


namespace cvk {
namespace {
    /**
     * @brief one queue, some devices have one queue only
     */
    constexpr std::array QUEUES{
        QueueFamilyProperties::GRAPHICS | QueueFamilyProperties::COMPUTE | QueueFamilyProperties::TRANSFER
    };
    constexpr std::array THREE_QUEUES{
        QueueFamilyProperties::GRAPHICS,
        QueueFamilyProperties::COMPUTE,
        QueueFamilyProperties::TRANSFER
    };
}

CVK_TEST(Device, base_constructor_does_not_create) {
    Instance const instance{test::instance_config(), create};
    auto const [physicalDevice, _] = test::pick(instance, QUEUES);

    Device const device{instance, physicalDevice};

    EXPECT_FALSE(device.created());
    EXPECT_TRUE(device.queues().empty());
}

CVK_TEST(Device, create_retrieves_the_queue) {
    Instance const instance{test::instance_config(), create};
    auto const [physicalDevice, familyIndices] = test::pick(instance, QUEUES);

    Device const device{instance, physicalDevice, Device::Config{familyIndices}};

    ASSERT_TRUE(device.created());
    EXPECT_EQ(&device.physicalDevice(), &physicalDevice);
    EXPECT_EQ(&device.instance(), &instance);
    ASSERT_EQ(device.queues().size(), 1);

    auto const& queue = device.queues()[0];
    ASSERT_NE(queue.get(), VK_NULL_HANDLE);
    EXPECT_EQ(queue.familyIndex(), familyIndices[0]);
    EXPECT_EQ(queue.index(), 0);
    EXPECT_TRUE(contains(queue.familyProperties(), QUEUES[0]));
    queue.waitIdle();

    device.waitIdle();
}

CVK_TEST(Device, create_retrieves_the_queues) {
    Instance const instance{test::instance_config(), create};

    auto const result = pick_physical_device(instance, {}, THREE_QUEUES, {});
    if(!result) GTEST_SKIP() << "needs a device with three queues";
    auto const& [physicalDevice, familyIndices] = *result;

    Device const device{instance, physicalDevice, Device::Config{familyIndices}};

    ASSERT_TRUE(device.created());
    ASSERT_EQ(device.queues().size(), THREE_QUEUES.size());

    std::map<queue_family_index_t, uint32_t> queueIndices{};
    for(auto const& [queue, familyIndex, properties] : std::views::zip(device.queues(), familyIndices, THREE_QUEUES)) {
        ASSERT_NE(queue.get(), VK_NULL_HANDLE);
        EXPECT_EQ(queue.familyIndex(), familyIndex);
        EXPECT_EQ(queue.index(), queueIndices[familyIndex]++);
        EXPECT_TRUE(contains(queue.familyProperties(), properties));
        queue.waitIdle();
    }

    device.waitIdle();
}

CVK_TEST(Device, queues_of_one_family_get_increasing_indices) {
    Instance const instance{test::instance_config(), create};
    auto const [physicalDevice, _] = test::pick(instance, QUEUES);

    auto const families = physicalDevice.queueFamilies();
    auto const family = std::ranges::find_if(families, [](auto const& f) { return f.vkProperties.queueCount >= 2; });
    if(family == families.end()) GTEST_SKIP() << "needs a queue family with two queues";

    Device const device{instance, physicalDevice, Device::Config{{family->index, family->index}}};

    ASSERT_EQ(device.queues().size(), 2);
    EXPECT_EQ(device.queues()[0].index(), 0);
    EXPECT_EQ(device.queues()[1].index(), 1);
    EXPECT_NE(device.queues()[0].get(), device.queues()[1].get());
}

CVK_TEST(Device, queues_work_after_move) {
    Instance const instance{test::instance_config(), create};
    auto const [physicalDevice, familyIndices] = test::pick(instance, QUEUES);

    Device device{instance, physicalDevice, Device::Config{familyIndices}};
    auto const vkDevice = device.get();

    Device const moved{std::move(device)};

    EXPECT_FALSE(device.created());
    ASSERT_TRUE(moved.created());
    EXPECT_EQ(moved.get(), vkDevice);

    for(auto const& queue : moved.queues())
        queue.waitIdle();
}

CVK_TEST(Device, release_and_wrap) {
    Instance const instance{test::instance_config(), create};
    auto const [physicalDevice, familyIndices] = test::pick(instance, QUEUES);

    Device device{instance, physicalDevice, Device::Config{familyIndices}};
    auto const vkDevice = device.get();

    auto state = device.release();

    EXPECT_FALSE(device.created());
    EXPECT_TRUE(device.queues().empty());
    EXPECT_EQ(state.vkDevice.get(), vkDevice);
    ASSERT_NE(state.functionTable, nullptr);
    EXPECT_EQ(state.queueFamilyIndices, familyIndices);

    Device const wrapped{instance, physicalDevice, std::move(state)};

    ASSERT_TRUE(wrapped.created());
    EXPECT_EQ(wrapped.get(), vkDevice);
    ASSERT_EQ(wrapped.queues().size(), familyIndices.size());
    for(auto const& queue : wrapped.queues())
        queue.waitIdle();
}

CVK_TEST(Device, wrap_without_table_loads_it) {
    Instance const instance{test::instance_config(), create};
    auto const [physicalDevice, familyIndices] = test::pick(instance, QUEUES);

    Device device{instance, physicalDevice, Device::Config{familyIndices}};
    auto state = device.release();
    state.functionTable = nullptr;

    Device const wrapped{instance, physicalDevice, std::move(state)};

    ASSERT_TRUE(wrapped.created());
    EXPECT_NE(wrapped.functions()->vkDestroyDevice, nullptr);
    wrapped.waitIdle();
}

CVK_TEST(Device, release_twice_is_safe) {
    Instance const instance{test::instance_config(), create};
    auto const [physicalDevice, familyIndices] = test::pick(instance, QUEUES);

    Device device{instance, physicalDevice, Device::Config{familyIndices}};
    Device first{instance, physicalDevice, device.release()};

    device.create(Device::Config{familyIndices});
    ASSERT_TRUE(device.created());
    EXPECT_NE(device.functions()->vkDestroyDevice, nullptr);

    Device const second{instance, physicalDevice, device.release()};
    EXPECT_TRUE(second.created());
}

CVK_TEST(Device, present_enables_swapchain_maintenance1) {
    Instance const instance{test::instance_config(test::headless_extensions()), create};
    std::array const surfaces{test::headless_surface(instance)};
    std::array const queues{QueueFamilyProperties::GRAPHICS | QueueFamilyProperties::PRESENT};

    auto const [physicalDevice, familyIndices] = test::pick(instance, queues, surfaces);

    Device const device{
        instance,
        physicalDevice,
        Device::Config{
            .queueFamilyIndices = familyIndices,
            .features = constants::PRESENT_DEVICE_FEATURES,
            .extensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME},
        }
    };

    ASSERT_TRUE(device.created());
    ASSERT_EQ(device.queues().size(), 1);
    EXPECT_TRUE(device.queues()[0].canPresent());
    EXPECT_TRUE(device.features().swapchainMaintenance1);

    // the feature bit adds its extension
    std::string const maintenance1{VK_KHR_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME};
    EXPECT_TRUE(std::ranges::contains(device.extensions(), maintenance1));
    EXPECT_NE(device.functions()->vkCreateSwapchainKHR, nullptr);
    EXPECT_NE(device.functions()->vkReleaseSwapchainImagesKHR, nullptr);
}

CVK_TEST(Device, move_assignment_destroys_the_old_device) {
    if constexpr(!constants::ENABLE_VALIDATION_LAYERS) GTEST_SKIP() << "needs the validation layers";

    test::reset_validation_errors();
    {
        Instance const instance{test::instance_config(), create, test::counting_messenger()};
        auto const [physicalDevice, familyIndices] = test::pick(instance, QUEUES);

        Device device{instance, physicalDevice, Device::Config{familyIndices}};
        Device other{instance, physicalDevice, Device::Config{familyIndices}};
        auto const vkDevice = other.get();

        device = std::move(other);

        ASSERT_EQ(device.get(), vkDevice);
        EXPECT_FALSE(other.created());
        ASSERT_EQ(device.queues().size(), 1);
        device.queues()[0].waitIdle();
        device.waitIdle();
    }
    EXPECT_EQ(test::validation_errors(), 0);
}
}
