#pragma once
#include "cvk/utility/device/physical_device_features.hpp"

#include <volk.h>

#include <array>
#include <string_view>

namespace cvk::constants {
/**
 * @brief extensions that every device enables
 */
inline constexpr std::array<std::string_view, 0> REQUIRED_DEVICE_EXTENSIONS{};

/**
 * @brief extensions that a device with a present queue enables
 */
inline constexpr std::array<std::string_view, 2> PRESENT_DEVICE_EXTENSIONS{
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_KHR_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME,
};

/**
 * @brief instance extensions that a device with a present queue requires, besides the platform surface extension
 */
inline constexpr std::array<std::string_view, 3> PRESENT_INSTANCE_EXTENSIONS{
    VK_KHR_SURFACE_EXTENSION_NAME,
    VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME,
    VK_KHR_SURFACE_MAINTENANCE_1_EXTENSION_NAME,
};

/**
 * @brief features that every device enables
 */
[[nodiscard]] inline utils::PhysicalDeviceFeatures required_device_features() {
    utils::PhysicalDeviceFeatures features{};

    features.features().features.samplerAnisotropy = VK_TRUE;

    features.features<VkPhysicalDeviceVulkan12Features>().timelineSemaphore = VK_TRUE;

    auto& features13 = features.features<VkPhysicalDeviceVulkan13Features>();
    features13.synchronization2 = VK_TRUE;
    features13.dynamicRendering = VK_TRUE;

    return features;
}
}
