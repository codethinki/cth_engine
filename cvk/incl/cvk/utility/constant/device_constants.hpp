#pragma once
#include "os_constants.hpp"

#include "cvk/utility/device/device_features.hpp"

#include <volk.h>

#include <algorithm>
#include <array>
#include <string_view>

namespace cvk::constants {
/**
 * features that every device enables
 * @details bindless:
 * - the descriptor indexing features that descriptorIndexing guarantees
 * - descriptorBindingVariableDescriptorCount
 * @details dynamicRenderingLocalRead: local reads in a dynamic rendering
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
 * features that a device with a present queue enables
 */
inline constexpr DeviceFeatures PRESENT_DEVICE_FEATURES{.swapchainMaintenance1 = true};

/**
 * extensions without feature bits that a device with a present queue enables
 * @details the swapchain and @ref OS_FENCE_DEVICE_EXTENSIONS for the exportable present fences
 */
inline constexpr auto PRESENT_DEVICE_EXTENSIONS = [] {
    std::array<std::string_view, 1 + OS_FENCE_DEVICE_EXTENSIONS.size()> extensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    std::ranges::copy(OS_FENCE_DEVICE_EXTENSIONS, extensions.begin() + 1);
    return extensions;
}();

/**
 * instance extensions that a device with a present queue requires, besides the platform surface extension
 */
inline constexpr std::array<std::string_view, 3> PRESENT_INSTANCE_EXTENSIONS{
    VK_KHR_SURFACE_EXTENSION_NAME,
    VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME,
    VK_KHR_SURFACE_MAINTENANCE_1_EXTENSION_NAME,
};
}
