#include "headless.hpp"
#include "test.hpp"

#include "cvk/base/instance.hpp"
#include "cvk/base/physical_device.hpp"
#include "cvk/utility/constant/device_constants.hpp"

#include <algorithm>
#include <array>
#include <string>
#include <vector>


namespace cvk {
namespace {
    /**
     * @brief one queue, some devices have one queue only
     */
    constexpr std::array QUEUES{
        QueueFamilyProperties::GRAPHICS | QueueFamilyProperties::COMPUTE | QueueFamilyProperties::TRANSFER
    };
    constexpr std::array PRESENT_QUEUES{QueueFamilyProperties::GRAPHICS | QueueFamilyProperties::PRESENT};

    std::vector<std::string> const UNKNOWN_EXTENSION{"VK_CVK_extension_that_does_not_exist"};
}

CVK_TEST(PhysicalDevice, pick_finds_the_queues) {
    Instance const instance{test::instance_config(), create};

    auto const pick = pick_physical_device(instance, {}, QUEUES, constants::BASE_DEVICE_FEATURES);

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
    Instance const instance{test::instance_config(), create};

    auto const [device, _] = test::pick(instance, QUEUES);

    EXPECT_TRUE(device.missing(constants::BASE_DEVICE_FEATURES).empty());
    EXPECT_TRUE(device.features().dynamicRenderingLocalRead);
    std::string const localRead{VK_KHR_DYNAMIC_RENDERING_LOCAL_READ_EXTENSION_NAME};
    EXPECT_TRUE(std::ranges::contains(device.extensions(), localRead));
}

CVK_TEST(PhysicalDevice, missing_names_the_extensions) {
    Instance const instance{test::instance_config(), create};
    auto const [device, _] = test::pick(instance, QUEUES);

    EXPECT_EQ(device.missing({}, UNKNOWN_EXTENSION), UNKNOWN_EXTENSION);
}

CVK_TEST(PhysicalDevice, pick_with_a_missing_extension_finds_nothing) {
    Instance const instance{test::instance_config(), create};

    EXPECT_FALSE(pick_physical_device(instance, {}, QUEUES, {}, UNKNOWN_EXTENSION).has_value());
}

CVK_TEST(PhysicalDevice, pick_present_without_surfaces_finds_nothing) {
    Instance const instance{test::instance_config(), create};

    EXPECT_FALSE(pick_physical_device(instance, {}, PRESENT_QUEUES, {}).has_value());
}

CVK_TEST(PhysicalDevice, pick_present_finds_a_family_of_the_surface) {
    Instance const instance{test::instance_config(test::headless_extensions()), create};
    std::array const surfaces{test::headless_surface(instance)};

    auto const [device, familyIndices] = test::pick(instance, PRESENT_QUEUES, surfaces);

    EXPECT_TRUE(device.features().swapchainMaintenance1);
    ASSERT_EQ(familyIndices.size(), 1);
    EXPECT_TRUE(surfaces[0].supportsFamily(device, familyIndices[0]));
}

CVK_TEST(PhysicalDevice, enumerate_without_surfaces_has_no_present) {
    Instance const instance{test::instance_config(), create};

    for(auto const& device : enumerate_physical_devices(instance))
        for(auto const& family : device.queueFamilies())
            EXPECT_FALSE(contains(family.properties, QueueFamilyProperties::PRESENT));
}

CVK_TEST(PhysicalDevice, copy_keeps_the_queried_members) {
    Instance const instance{test::instance_config(), create};
    auto const [device, _] = test::pick(instance, QUEUES);

    PhysicalDevice const copy{device};

    EXPECT_EQ(copy.get(), device.get());
    EXPECT_EQ(copy.features(), device.features());
    EXPECT_TRUE(std::ranges::equal(copy.extensions(), device.extensions()));
    EXPECT_TRUE(std::ranges::equal(copy.queueFamilies(), device.queueFamilies()));
    EXPECT_EQ(copy.maxSampleCount(), device.maxSampleCount());
    EXPECT_STREQ(copy.properties().deviceName, device.properties().deviceName);
}

CVK_TEST(PhysicalDevice, find_supported_format) {
    Instance const instance{test::instance_config(), create};
    auto const [device, _] = test::pick(instance, QUEUES);

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
    EXPECT_THROW(findNone(), cth::except::default_exception);
}
}
