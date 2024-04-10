#include "CthSurface.hpp"
#include "CthWindow.hpp"
#include "vulkan/base/CthInstance.hpp"
#include "vulkan/utility/CthVkUtils.hpp"

#include <cth/cth_log.hpp>

#include <GLFW/glfw3.h>

namespace cth {
Surface::Surface(VkSurfaceKHR vk_surface, Instance* instance) : vkSurface(vk_surface), instance(instance) {}
Surface::~Surface() {
    vkDestroySurfaceKHR(instance->get(), vkSurface, nullptr);
    log::msg("destroyed surface");
}
vector<VkPresentModeKHR> Surface::presentModes(VkPhysicalDevice physical_device) const {
    uint32_t size;
    const VkResult result1 = vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, vkSurface, &size, nullptr);

    CTH_STABLE_ERR(result1 != VK_SUCCESS, "device-surface present modes query failed")
        throw cth::except::vk_result_exception{result1, details->exception()};

    if(!size) return {};

    vector<VkPresentModeKHR> modes(size);
    const VkResult result2 = vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, vkSurface, &size, modes.data());

    CTH_STABLE_ERR(result2 != VK_SUCCESS, "device-surface present modes query failed")
        throw cth::except::vk_result_exception{result2, details->exception()};

    return modes;
}

vector<VkSurfaceFormatKHR> Surface::formats(VkPhysicalDevice physical_device) const {
    uint32_t size;
    const VkResult result1 = vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, vkSurface, &size, nullptr);

    CTH_STABLE_ERR(result1 != VK_SUCCESS, "device-surface formats query failed")
        throw except::vk_result_exception{result1, details->exception()};

    if(!size) return {};
    vector<VkSurfaceFormatKHR> formats(size);
    const VkResult result2 = vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, vkSurface, &size, formats.data());
    CTH_STABLE_ERR(result2 != VK_SUCCESS, "device-surface formats query failed")
        throw except::vk_result_exception{result2, details->exception()};

    return formats;
}

VkSurfaceCapabilitiesKHR Surface::capabilities(VkPhysicalDevice physical_device) const {
    VkSurfaceCapabilitiesKHR capabilities;
    const auto result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, vkSurface, &capabilities);

    CTH_STABLE_ERR(result != VK_SUCCESS, "device-surface capabilities query failed")
        throw except::vk_result_exception{result, details->exception()};

    return capabilities;
}
VkBool32 Surface::support(VkPhysicalDevice physical_device, const uint32_t family_index) const {
    VkBool32 support;
    const VkResult result = vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, family_index, vkSurface, &support);
    CTH_STABLE_ERR(result != VK_SUCCESS, "device-surface support query failed")
        throw except::vk_result_exception{result, details->exception()};


    return support;
}



}
