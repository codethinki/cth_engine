#pragma once
#include "cvk/utility/device/physical_device_features.hpp"

#include <volk.h>

#include <array>
#include <string_view>

namespace cvk::constants {
/**
 * @brief extensions that every device enables
 * @details VK_KHR_dynamic_rendering_local_read: tile local reads in a dynamic rendering, like subpasses
 */
inline constexpr std::array<std::string_view, 1> REQUIRED_DEVICE_EXTENSIONS{
    VK_KHR_DYNAMIC_RENDERING_LOCAL_READ_EXTENSION_NAME,
};

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
 * @details bindless: descriptorIndexing guarantees the minimum set of descriptor indexing features, they are enabled,
 *  descriptorBindingVariableDescriptorCount is not part of the set, the one extra requirement
 */
[[nodiscard]] inline utils::PhysicalDeviceFeatures required_device_features() {
    utils::PhysicalDeviceFeatures features{};

    auto& features10 = features.features().features;
    features10.samplerAnisotropy = VK_TRUE;
    features10.shaderSampledImageArrayDynamicIndexing = VK_TRUE;
    features10.shaderStorageBufferArrayDynamicIndexing = VK_TRUE;

    auto& features12 = features.features<VkPhysicalDeviceVulkan12Features>();
    features12.timelineSemaphore = VK_TRUE;
    features12.descriptorIndexing = VK_TRUE;
    features12.shaderUniformTexelBufferArrayDynamicIndexing = VK_TRUE;
    features12.shaderStorageTexelBufferArrayDynamicIndexing = VK_TRUE;
    features12.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
    features12.shaderStorageBufferArrayNonUniformIndexing = VK_TRUE;
    features12.shaderUniformTexelBufferArrayNonUniformIndexing = VK_TRUE;
    features12.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
    features12.descriptorBindingStorageImageUpdateAfterBind = VK_TRUE;
    features12.descriptorBindingStorageBufferUpdateAfterBind = VK_TRUE;
    features12.descriptorBindingUniformTexelBufferUpdateAfterBind = VK_TRUE;
    features12.descriptorBindingStorageTexelBufferUpdateAfterBind = VK_TRUE;
    features12.descriptorBindingUpdateUnusedWhilePending = VK_TRUE;
    features12.descriptorBindingPartiallyBound = VK_TRUE;
    features12.runtimeDescriptorArray = VK_TRUE;
    features12.descriptorBindingVariableDescriptorCount = VK_TRUE;

    auto& features13 = features.features<VkPhysicalDeviceVulkan13Features>();
    features13.synchronization2 = VK_TRUE;
    features13.dynamicRendering = VK_TRUE;

    features.features<VkPhysicalDeviceDynamicRenderingLocalReadFeaturesKHR>().dynamicRenderingLocalRead = VK_TRUE;

    return features;
}
}
