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
using test::APP_NAME;
using test::NO_EXTENSIONS;

namespace {
    /**
     * @brief one queue, some devices have one queue only
     */
    constexpr std::array QUEUES{
        QueueFamilyProperties::GRAPHICS | QueueFamilyProperties::COMPUTE | QueueFamilyProperties::TRANSFER
    };
    constexpr std::array PRESENT_QUEUES{QueueFamilyProperties::GRAPHICS | QueueFamilyProperties::PRESENT};

    bool requires_extension(PhysicalDevice const& device, std::string_view extension) {
        return std::ranges::contains(device.requiredExtensions(), extension);
    }
}

CVK_TEST(PhysicalDevice, auto_pick_finds_the_queues) {
    Instance const instance{APP_NAME, NO_EXTENSIONS, std::nullopt};

    auto const result = PhysicalDevice::AutoPick(instance, {}, QUEUES);

    ASSERT_TRUE(result.has_value());
    auto const& [device, familyIndices] = *result;

    ASSERT_TRUE(device.created());
    ASSERT_EQ(familyIndices.size(), QUEUES.size());

    for(auto const [familyIndex, properties] : std::views::zip(familyIndices, QUEUES)) {
        auto const* family = device.queueFamily(familyIndex);
        ASSERT_NE(family, nullptr);
        EXPECT_TRUE(contains(family->properties, properties));
    }
}

CVK_TEST(PhysicalDevice, auto_pick_adds_the_defaults) {
    Instance const instance{APP_NAME, NO_EXTENSIONS, std::nullopt};

    auto const [device, _] = test::pick(instance, QUEUES);

    EXPECT_TRUE(device.supports(constants::required_device_features()).empty());
    EXPECT_TRUE(device.requiredFeatures().supports(constants::required_device_features()).empty());

    for(auto const extension : constants::PRESENT_DEVICE_EXTENSIONS)
        EXPECT_FALSE(requires_extension(device, extension)) << extension;
    for(auto const extension : constants::REQUIRED_DEVICE_EXTENSIONS)
        EXPECT_TRUE(requires_extension(device, extension)) << extension;
}

CVK_TEST(PhysicalDevice, queries_the_features_of_its_extensions) {
    Instance const instance{APP_NAME, NO_EXTENSIONS, std::nullopt};

    auto const [device, _] = test::pick(instance, QUEUES);

    using local_read_t = VkPhysicalDeviceDynamicRenderingLocalReadFeaturesKHR;
    EXPECT_TRUE(device.features().linked<local_read_t>());
    EXPECT_TRUE(device.features().features<local_read_t>().dynamicRenderingLocalRead);
}

CVK_TEST(PhysicalDevice, auto_pick_present_without_surfaces_finds_nothing) {
    Instance const instance{APP_NAME, NO_EXTENSIONS, std::nullopt};

    EXPECT_FALSE(PhysicalDevice::AutoPick(instance, {}, PRESENT_QUEUES).has_value());
}

CVK_TEST(PhysicalDevice, auto_pick_present_requires_the_present_extensions) {
    Instance const instance{APP_NAME, test::headless_extensions(), std::nullopt};
    std::array const surfaces{test::headless_surface(instance)};

    auto const [device, familyIndices] = test::pick(instance, PRESENT_QUEUES, surfaces);

    for(auto const extension : constants::PRESENT_DEVICE_EXTENSIONS)
        EXPECT_TRUE(requires_extension(device, extension)) << extension;

    ASSERT_EQ(familyIndices.size(), 1);
    EXPECT_TRUE(surfaces[0].supportsFamily(device, familyIndices[0]));
}

CVK_TEST(PhysicalDevice, auto_pick_missing_extension_finds_nothing) {
    Instance const instance{APP_NAME, NO_EXTENSIONS, std::nullopt};
    std::vector<std::string> const extensions{"VK_CVK_extension_that_does_not_exist"};

    EXPECT_FALSE(PhysicalDevice::AutoPick(instance, {}, QUEUES, extensions).has_value());
}

CVK_TEST(PhysicalDevice, query_queue_families_without_surfaces_has_no_present) {
    Instance const instance{APP_NAME, NO_EXTENSIONS, std::nullopt};

    for(auto const vkDevice : PhysicalDevice::enumerateVkDevices(instance.get()))
        for(auto const& family : PhysicalDevice::queryQueueFamilies({}, vkDevice))
            EXPECT_FALSE(contains(family.properties, QueueFamilyProperties::PRESENT));
}

CVK_TEST(PhysicalDevice, wrap_queries_the_missing_members) {
    Instance const instance{APP_NAME, NO_EXTENSIONS, std::nullopt};
    auto const [picked, _] = test::pick(instance, QUEUES);

    PhysicalDevice const device{
        instance,
        picked.requiredFeatures(),
        picked.requiredExtensions(),
        PhysicalDevice::State{.vkDevice = picked.get(), .queueFamilies = {}}
    };

    ASSERT_TRUE(device.created());
    EXPECT_EQ(device.get(), picked.get());
    EXPECT_EQ(device.maxSampleCount(), picked.maxSampleCount());
    EXPECT_EQ(device.queueFamilies().size(), picked.queueFamilies().size());
    EXPECT_TRUE(std::ranges::equal(device.extensions(), picked.extensions()));
    EXPECT_STREQ(device.properties().deviceName, picked.properties().deviceName);
}

CVK_TEST(PhysicalDevice, copy_keeps_the_queried_members) {
    Instance const instance{APP_NAME, NO_EXTENSIONS, std::nullopt};
    auto const [device, _] = test::pick(instance, QUEUES);

    PhysicalDevice const copy{device};

    EXPECT_EQ(copy.get(), device.get());
    EXPECT_TRUE(std::ranges::equal(copy.queueFamilies(), device.queueFamilies()));
    EXPECT_TRUE(copy.supports(device.requiredFeatures()).empty());
    EXPECT_EQ(copy.features().get()->pNext, &copy.features().features<VkPhysicalDeviceVulkan11Features>());
}

CVK_TEST(PhysicalDevice, find_supported_format) {
    Instance const instance{APP_NAME, NO_EXTENSIONS, std::nullopt};
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
