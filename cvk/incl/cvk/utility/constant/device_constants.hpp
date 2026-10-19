#pragma once
#include "cvk/utility/device/device_features.hpp"

#include <volk.h>

#include <array>
#include <string_view>

namespace cvk::constants {
/**
 * @brief features that every device enables
 * @details bindless: descriptorIndexing guarantees the minimum set of descriptor indexing features, they are enabled,
 *  descriptorBindingVariableDescriptorCount is not part of the set, the one extra requirement,
 *  dynamicRenderingLocalRead: tile local reads in a dynamic rendering, like subpasses
 */
inline constexpr DeviceFeatures BASE_DEVICE_FEATURES{
    .samplerAnisotropy = true,
    .shaderSampledImageArrayDynamicIndexing = true,
    .shaderStorageBufferArrayDynamicIndexing = true,
    .descriptorIndexing = true,
    .shaderUniformTexelBufferArrayDynamicIndexing = true,
    .shaderStorageTexelBufferArrayDynamicIndexing = true,
    .shaderSampledImageArrayNonUniformIndexing = true,
    .shaderStorageBufferArrayNonUniformIndexing = true,
    .shaderUniformTexelBufferArrayNonUniformIndexing = true,
    .descriptorBindingSampledImageUpdateAfterBind = true,
    .descriptorBindingStorageImageUpdateAfterBind = true,
    .descriptorBindingStorageBufferUpdateAfterBind = true,
    .descriptorBindingUniformTexelBufferUpdateAfterBind = true,
    .descriptorBindingStorageTexelBufferUpdateAfterBind = true,
    .descriptorBindingUpdateUnusedWhilePending = true,
    .descriptorBindingPartiallyBound = true,
    .descriptorBindingVariableDescriptorCount = true,
    .runtimeDescriptorArray = true,
    .timelineSemaphore = true,
    .synchronization2 = true,
    .dynamicRendering = true,
    .dynamicRenderingLocalRead = true,
};

/**
 * @brief features that a device with a present queue enables
 */
inline constexpr DeviceFeatures PRESENT_DEVICE_FEATURES{.swapchainMaintenance1 = true};

/**
 * @brief extensions without feature bits that a device with a present queue enables
 */
inline constexpr std::array<std::string_view, 1> PRESENT_DEVICE_EXTENSIONS{
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
};

/**
 * @brief instance extensions that a device with a present queue requires, besides the platform surface extension
 */
inline constexpr std::array<std::string_view, 3> PRESENT_INSTANCE_EXTENSIONS{
    VK_KHR_SURFACE_EXTENSION_NAME,
    VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME,
    VK_KHR_SURFACE_MAINTENANCE_1_EXTENSION_NAME,
};
}
