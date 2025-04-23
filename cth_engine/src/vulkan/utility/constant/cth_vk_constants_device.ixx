module;
#include "lib/volk.hpp"
export module cth.vk.constants.device;

export namespace cth::vk::constants {

constexpr VkPhysicalDeviceFeatures REQUIRED_DEVICE_FEATURES{
    .robustBufferAccess = false,
    .samplerAnisotropy = true,
};

std::array<std::string, 2> const REQUIRED_DEVICE_EXTENSIONS{
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME,
};

constexpr VkPhysicalDeviceTimelineSemaphoreFeatures TIMELINE_SEMAPHORE_FEATURE{
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES_KHR,
    .timelineSemaphore = true
};

VkPhysicalDeviceFeatures2 const REQUIRED_DEVICE_FEATURES2{
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR,
    .pNext = const_cast<void*>(reinterpret_cast<void const*>(&TIMELINE_SEMAPHORE_FEATURE)),
    .features = REQUIRED_DEVICE_FEATURES,
};

} //namespace cth::vk::constants