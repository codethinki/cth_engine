#include "CthSurface.hpp"

#include "CthOSWindow.hpp"
#include "vulkan/base/CthPhysicalDevice.hpp"
#include "vulkan/utility/cth_vk_utils.hpp"



namespace cth::vk {
using std::vector;

Surface::~Surface() {
    vkDestroySurfaceKHR(_instance->get(), _handle.get(), nullptr);
    log::msg("destroyed surface");
}
bool Surface::supportsFamily(PhysicalDevice const& physical_device, uint32_t const family_index) const {
    VkBool32 support = false;
    VkResult const result = vkGetPhysicalDeviceSurfaceSupportKHR(physical_device.get(), family_index, _handle.get(), &support);
    CTH_STABLE_ERR(result != VK_SUCCESS, "device-surface support query failed")
        throw except::vk_result_exception{result, details->exception()};
    return support;
}
vector<VkPresentModeKHR> Surface::presentModes(PhysicalDevice const& physical_device) const {
    uint32_t size = 0;
    VkResult const result1 = vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device.get(), _handle.get(), &size, nullptr);

    CTH_STABLE_ERR(result1 != VK_SUCCESS, "device-surface present modes query failed")
        throw except::vk_result_exception{result1, details->exception()};

    if(!size) return {};

    vector<VkPresentModeKHR> modes(size);
    VkResult const result2 = vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device.get(), _handle.get(), &size, modes.data());

    CTH_STABLE_ERR(result2 != VK_SUCCESS, "device-surface present modes query failed")
        throw except::vk_result_exception{result2, details->exception()};

    return modes;
}
vector<VkSurfaceFormatKHR> Surface::formats(PhysicalDevice const& physical_device) const {
    uint32_t size = 0;
    VkResult const result1 = vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device.get(), _handle.get(), &size, nullptr);

    CTH_STABLE_ERR(result1 != VK_SUCCESS, "device-surface formats query failed")
        throw except::vk_result_exception{result1, details->exception()};

    if(!size) return {};

    vector<VkSurfaceFormatKHR> formats(size);

    VkResult const result2 = vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device.get(), _handle.get(), &size, formats.data());
    CTH_STABLE_ERR(result2 != VK_SUCCESS, "device-surface formats query failed")
        throw except::vk_result_exception{result2, details->exception()};

    return formats;
}

VkSurfaceCapabilitiesKHR Surface::capabilities(PhysicalDevice const& physical_device) const {
    VkSurfaceCapabilitiesKHR capabilities;
    auto const result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device.get(), _handle.get(), &capabilities);

    CTH_STABLE_ERR(result != VK_SUCCESS, "device-surface capabilities query failed")
        throw except::vk_result_exception{result, details->exception()};

    return capabilities;
}
Surface Surface::Temp(BasicInstance const* instance) { return Surface{instance, OSWindow::tempSurface(instance)}; }

void Surface::debug_check(Surface const* surface) {
    CTH_ERR(surface == nullptr, "surface invalid (nullptr)") throw details->exception();
    CTH_ERR(surface->get() == VK_NULL_HANDLE, "surface handle invalid (VK_NULL_HANDLE)")
        throw details->exception();
}



}
