#include "test.hpp"

#include "cvk/utility/device/physical_device_features.hpp"

#include <cstddef>


namespace cvk::utils {
namespace {
    void expect_linked(PhysicalDeviceFeatures const& features) {
        auto const* const features2 = features.get();

        EXPECT_EQ(features2, &features.features());
        EXPECT_EQ(features2->sType, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2);
        EXPECT_EQ(features2->pNext, &features.features<VkPhysicalDeviceVulkan11Features>());
        EXPECT_EQ(
            features.features<VkPhysicalDeviceVulkan11Features>().pNext,
            &features.features<VkPhysicalDeviceVulkan12Features>()
        );
        EXPECT_EQ(
            features.features<VkPhysicalDeviceVulkan12Features>().pNext,
            &features.features<VkPhysicalDeviceVulkan13Features>()
        );
        EXPECT_EQ(features.features<VkPhysicalDeviceVulkan13Features>().pNext, nullptr);
    }

    constexpr size_t SAMPLER_ANISOTROPY_INDEX = offsetof(VkPhysicalDeviceFeatures, samplerAnisotropy) / sizeof(VkBool32);
    constexpr size_t SYNCHRONIZATION2_INDEX =
        (offsetof(VkPhysicalDeviceVulkan13Features, synchronization2)
            - dev::feature_flags<VkPhysicalDeviceVulkan13Features>::OFFSET) / sizeof(VkBool32);
}

CVK_TEST(PhysicalDeviceFeatures, default_is_linked_and_false) {
    PhysicalDeviceFeatures const features{};

    expect_linked(features);

    EXPECT_FALSE(features.features().features.samplerAnisotropy);
    EXPECT_FALSE(features.features<VkPhysicalDeviceVulkan13Features>().synchronization2);
}

CVK_TEST(PhysicalDeviceFeatures, copy_links_its_own_chain) {
    PhysicalDeviceFeatures features{};
    features.features<VkPhysicalDeviceVulkan12Features>().timelineSemaphore = VK_TRUE;

    PhysicalDeviceFeatures const copy{features};
    expect_linked(copy);
    EXPECT_TRUE(copy.features<VkPhysicalDeviceVulkan12Features>().timelineSemaphore);

    PhysicalDeviceFeatures assigned{};
    assigned = copy;
    expect_linked(assigned);
    EXPECT_TRUE(assigned.features<VkPhysicalDeviceVulkan12Features>().timelineSemaphore);
}

CVK_TEST(PhysicalDeviceFeatures, move_links_its_own_chain) {
    PhysicalDeviceFeatures features{};
    features.features<VkPhysicalDeviceVulkan11Features>().multiview = VK_TRUE;

    PhysicalDeviceFeatures const moved{std::move(features)};
    expect_linked(moved);
    EXPECT_TRUE(moved.features<VkPhysicalDeviceVulkan11Features>().multiview);
}

CVK_TEST(PhysicalDeviceFeatures, supports_reports_missing_features) {
    PhysicalDeviceFeatures available{};
    available.features<VkPhysicalDeviceVulkan12Features>().timelineSemaphore = VK_TRUE;

    PhysicalDeviceFeatures required{};
    required.features().features.samplerAnisotropy = VK_TRUE;
    required.features<VkPhysicalDeviceVulkan12Features>().timelineSemaphore = VK_TRUE;
    required.features<VkPhysicalDeviceVulkan13Features>().synchronization2 = VK_TRUE;

    auto const missing = available.supports(required);

    ASSERT_EQ(missing.size(), 2);
    EXPECT_EQ(missing[0].sType, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2);
    EXPECT_EQ(missing[0].index, SAMPLER_ANISOTROPY_INDEX);
    EXPECT_EQ(missing[1].sType, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES);
    EXPECT_EQ(missing[1].index, SYNCHRONIZATION2_INDEX);
}

CVK_TEST(PhysicalDeviceFeatures, supports_itself) {
    PhysicalDeviceFeatures features{};
    features.features().features.samplerAnisotropy = VK_TRUE;
    features.features<VkPhysicalDeviceVulkan13Features>().dynamicRendering = VK_TRUE;

    EXPECT_TRUE(features.supports(features).empty());
}

CVK_TEST(PhysicalDeviceFeatures, merge_enables_the_features_of_both) {
    PhysicalDeviceFeatures features{};
    features.features().features.samplerAnisotropy = VK_TRUE;

    PhysicalDeviceFeatures other{};
    other.features<VkPhysicalDeviceVulkan13Features>().synchronization2 = VK_TRUE;

    features.merge(other);

    EXPECT_TRUE(features.features().features.samplerAnisotropy);
    EXPECT_TRUE(features.features<VkPhysicalDeviceVulkan13Features>().synchronization2);
    EXPECT_FALSE(features.features<VkPhysicalDeviceVulkan13Features>().dynamicRendering);
    expect_linked(features);
}
}
