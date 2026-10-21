#include "context.hpp"
#include "headless.hpp"
#include "test.hpp"

#include "cvk/base/instance.hpp"
#include "cvk/base/physical_device.hpp"
#include "cvk/utility/constant/device_constants.hpp"
#include "cvk/utility/vk_exceptions.hpp"

#include <algorithm>
#include <array>
#include <span>
#include <string>
#include <vector>


namespace cvk {
namespace {
    using test::Context;
    using test::QUEUES;

    constexpr std::array PRESENT_QUEUES{QueueFamilyProperties::GRAPHICS | QueueFamilyProperties::PRESENT};

    std::vector<std::string> const UNKNOWN_EXTENSION{"VK_CVK_extension_that_does_not_exist"};
}

CVK_TEST(PhysicalDevice, pick_finds_the_queues) {
    Context const context{};

    auto const pick = pick_physical_device(context.instance, {}, QUEUES, constants::BASE_DEVICE_FEATURES);

    ASSERT_TRUE(pick.has_value());
    auto const& [device, familyIndices] = *pick;
    ASSERT_EQ(familyIndices.size(), QUEUES.size());

    for(auto const [familyIndex, properties] : std::views::zip(familyIndices, QUEUES)) {
        auto const* family = device.queueFamily(familyIndex);
        ASSERT_NE(family, nullptr);
        EXPECT_TRUE(contains(family->properties, properties));
    }
}

CVK_TEST(PhysicalDevice, picked_device_has_the_features) {
    Context const context{};

    auto const [device, _] = test::pick(context.instance, QUEUES);

    EXPECT_TRUE(device.missing(constants::BASE_DEVICE_FEATURES).empty());
    EXPECT_TRUE(device.features().dynamicRenderingLocalRead);
    std::string const localRead{VK_KHR_DYNAMIC_RENDERING_LOCAL_READ_EXTENSION_NAME};
    EXPECT_TRUE(std::ranges::contains(device.extensions(), localRead));
}

CVK_TEST(PhysicalDevice, missing_names_the_extensions) {
    Context const context{};
    auto const [device, _] = test::pick(context.instance, QUEUES);

    EXPECT_EQ(device.missing({}, UNKNOWN_EXTENSION), UNKNOWN_EXTENSION);
}

CVK_TEST(PhysicalDevice, pick_with_a_missing_extension_finds_nothing) {
    Context const context{};

    EXPECT_FALSE(pick_physical_device(context.instance, {}, QUEUES, {}, UNKNOWN_EXTENSION).has_value());
}

CVK_TEST(PhysicalDevice, pick_present_without_surfaces_finds_nothing) {
    Context const context{};

    EXPECT_FALSE(pick_physical_device(context.instance, {}, PRESENT_QUEUES, {}).has_value());
}

CVK_TEST(PhysicalDevice, pick_present_finds_a_family_of_the_surface) {
    test::PresentContext const context{};
    std::span const surfaces{&context.surface(), 1};

    auto const [device, familyIndices] = test::pick(context.instance, PRESENT_QUEUES, surfaces);

    EXPECT_TRUE(device.features().swapchainMaintenance1);
    ASSERT_EQ(familyIndices.size(), 1);
    EXPECT_TRUE(context.surface().supportsFamily(device, familyIndices[0]));
}

CVK_TEST(PhysicalDevice, enumerate_without_surfaces_has_no_present) {
    Context const context{};

    for(auto const& device : enumerate_physical_devices(context.instance))
        for(auto const& family : device.queueFamilies())
            EXPECT_FALSE(contains(family.properties, QueueFamilyProperties::PRESENT));
}

CVK_TEST(PhysicalDevice, copy_keeps_the_queried_members) {
    Context const context{};
    auto const [device, _] = test::pick(context.instance, QUEUES);

    PhysicalDevice const copy{device};

    EXPECT_EQ(copy.get(), device.get());
    EXPECT_EQ(copy.features(), device.features());
    EXPECT_TRUE(std::ranges::equal(copy.extensions(), device.extensions()));
    EXPECT_TRUE(std::ranges::equal(copy.queueFamilies(), device.queueFamilies()));
    EXPECT_EQ(copy.maxSampleCount(), device.maxSampleCount());
    EXPECT_STREQ(copy.properties().deviceName, device.properties().deviceName);
    EXPECT_EQ(copy.uuid(), device.uuid());
}

CVK_TEST(PhysicalDevice, uuid_is_the_device_uuid) {
    Context const context{};
    auto const& device = context.core.physicalDevice();

    VkPhysicalDeviceIDProperties idProperties{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES,
        .pNext = nullptr,
    };
    VkPhysicalDeviceProperties2 properties{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
        .pNext = &idProperties,
    };
    vkGetPhysicalDeviceProperties2(device.get(), &properties);

    EXPECT_TRUE(std::ranges::equal(device.uuid(), idProperties.deviceUUID));
    EXPECT_STREQ(device.properties().deviceName, properties.properties.deviceName);
}

CVK_TEST(PhysicalDevice, type_rank_prefers_discrete_integrated_virtual_cpu_other) {
    constexpr auto rank = dev::device_type_rank;
    static_assert(rank(VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) == 0);
    static_assert(rank(VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) < rank(VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU));
    static_assert(rank(VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) < rank(VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU));
    static_assert(rank(VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU) < rank(VK_PHYSICAL_DEVICE_TYPE_CPU));
    static_assert(rank(VK_PHYSICAL_DEVICE_TYPE_CPU) < rank(VK_PHYSICAL_DEVICE_TYPE_OTHER));
}

CVK_TEST(PhysicalDevice, find_supported_format) {
    Context const context{};
    auto const& device = context.core.physicalDevice();

    // R8G8B8A8_UNORM sampled images with optimal tiling are mandatory
    std::array const candidates{VK_FORMAT_R8G8B8A8_UNORM};

    auto const format = device.findSupportedFormat(
        candidates,
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT
    );
    EXPECT_EQ(format, VK_FORMAT_R8G8B8A8_UNORM);

    auto const findNone = [&] {
        [[maybe_unused]] auto const none = device.findSupportedFormat(
            {},
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT
        );
    };
    EXPECT_THROW(findNone(), cvk_exception);
}
}
