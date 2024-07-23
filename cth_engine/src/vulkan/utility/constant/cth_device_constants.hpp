#pragma once
namespace cth::vk::constants {
static std::array<std::string, 3> const REQUIRED_DEVICE_EXTENSIONS = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME,
};

static constexpr VkPhysicalDeviceFeatures REQUIRED_DEVICE_FEATURES{
    .samplerAnisotropy = true,
};
static VkPhysicalDeviceTimelineSemaphoreFeatures TIMELINE_SEMAPHORE_FEATURE{
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES_KHR,
    .timelineSemaphore = true
};

static constexpr VkPhysicalDeviceFeatures2 REQUIRED_DEVICE_FEATURES2{
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR,
    .pNext = reinterpret_cast<void*>(&TIMELINE_SEMAPHORE_FEATURE),
    .features = REQUIRED_DEVICE_FEATURES,
};

} //namespace cth::vk::constants
