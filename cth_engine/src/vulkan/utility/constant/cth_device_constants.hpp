#pragma once
namespace cth::vk::constants {
static constexpr std::array<std::string_view, 3> REQUIRED_DEVICE_EXTENSIONS = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
    VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME,
};

static constexpr VkPhysicalDeviceFeatures REQUIRED_DEVICE_FEATURES{
    .samplerAnisotropy = true,
};
static constexpr VkPhysicalDeviceTimelineSemaphoreFeatures TIMELINE_SEMAPHORE_FEATURE{
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES_KHR,
    .timelineSemaphore = true
};

static constexpr VkPhysicalDeviceFeatures2 REQUIRED_DEVICE_FEATURES2{
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR,
    .pNext = (void*) (&TIMELINE_SEMAPHORE_FEATURE),
    .features = REQUIRED_DEVICE_FEATURES,
};

} //namespace cth::vk::constants
