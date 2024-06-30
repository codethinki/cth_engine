#include "CthSurface.hpp"

#include "CthOSWindow.hpp"
#include "vulkan/base/CthPhysicalDevice.hpp"
#include "vulkan/utility/CthVkUtils.hpp"

#include <vulkan/vulkan.h>


namespace cth {
using std::vector;

Surface::~Surface() {
    vkDestroySurfaceKHR(_instance->get(), _handle.get(), nullptr);
    log::msg("destroyed surface");
}
bool Surface::supportsFamily(const PhysicalDevice& physical_device, const uint32_t family_index) const {
    VkBool32 support = false;
    const VkResult result = vkGetPhysicalDeviceSurfaceSupportKHR(physical_device.get(), family_index, _handle.get(), &support);
    CTH_STABLE_ERR(result != VK_SUCCESS, "device-surface support query failed")
        throw except::vk_result_exception{result, details->exception()};
    return support;
}
vector<VkPresentModeKHR> Surface::presentModes(const PhysicalDevice& physical_device) const {
    uint32_t size = 0;
    const VkResult result1 = vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device.get(), _handle.get(), &size, nullptr);

    CTH_STABLE_ERR(result1 != VK_SUCCESS, "device-surface present modes query failed")
        throw except::vk_result_exception{result1, details->exception()};

    if(!size) return {};

    vector<VkPresentModeKHR> modes(size);
    const VkResult result2 = vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device.get(), _handle.get(), &size, modes.data());

    CTH_STABLE_ERR(result2 != VK_SUCCESS, "device-surface present modes query failed")
        throw except::vk_result_exception{result2, details->exception()};

    return modes;
}
vector<VkSurfaceFormatKHR> Surface::formats(const PhysicalDevice& physical_device) const {
    uint32_t size = 0;
    const VkResult result1 = vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device.get(), _handle.get(), &size, nullptr);

    CTH_STABLE_ERR(result1 != VK_SUCCESS, "device-surface formats query failed")
        throw except::vk_result_exception{result1, details->exception()};

    if(!size) return {};

    vector<VkSurfaceFormatKHR> formats(size);

    const VkResult result2 = vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device.get(), _handle.get(), &size, formats.data());
    CTH_STABLE_ERR(result2 != VK_SUCCESS, "device-surface formats query failed")
        throw except::vk_result_exception{result2, details->exception()};

    return formats;
}

VkSurfaceCapabilitiesKHR Surface::capabilities(const PhysicalDevice& physical_device) const {
    VkSurfaceCapabilitiesKHR capabilities;
    const auto result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device.get(), _handle.get(), &capabilities);

    CTH_STABLE_ERR(result != VK_SUCCESS, "device-surface capabilities query failed")
        throw except::vk_result_exception{result, details->exception()};

    return capabilities;
}
Surface Surface::Temp(const BasicInstance* instance) {
    return Surface{instance, OSWindow::tempSurface(instance)};
}

void Surface::debug_check(const Surface* surface) {
    CTH_ERR(surface == nullptr, "surface invalid (nullptr)");
    CTH_ERR(surface->get() == VK_NULL_HANDLE, "surface handle invalid (VK_NULL_HANDLE)");
}



}
