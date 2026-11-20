#include "cvk/surface/surface.hpp"

#include "cvk/base/instance.hpp"
#include "cvk/base/physical_device.hpp"
#include "cvk/surface/swapchain.hpp"
#include "cvk/utility/enumerate.hpp"
#include "cvk/utility/vk_exceptions.hpp"

#include <limits>
#include <utility>


namespace cvk {
using std::vector;

Surface::Surface(Instance const& instance) : _instance{instance} {}

Surface::Surface(Instance const& instance, State const& state) : Surface{instance} { wrap(state); }

void Surface::wrap(State const& state) {
    Instance::debug_check(*_instance);
    debug_check_handle(state.vkSurface);
    optDestroy();

    _handle.reset(state.vkSurface.get(), _instance->get());
    _extentCallback = state.extentCallback;
}

void Surface::destroy() {
    debug_check(*this);

    _handle.reset();

    forget();
}

Surface::State Surface::release() {
    debug_check(*this);
    State state{.vkSurface = _handle.release(), .extentCallback = std::move(_extentCallback)};

    forget();
    return state;
}

bool Surface::supportsFamily(PhysicalDevice const& physicalDevice, uint32_t familyIndex) const {
    debug_check(*this);

    VkBool32 support = false;
    auto const result = vkGetPhysicalDeviceSurfaceSupportKHR(
        physicalDevice.get(),
        familyIndex,
        _handle.get(),
        &support
    );
    CVK_RESULT_STABLE_THROW(result != VK_SUCCESS, result, "device-surface support query failed") {}

    return support;
}

vector<VkPresentModeKHR> Surface::presentModes(PhysicalDevice const& physicalDevice) const {
    debug_check(*this);

    return dev::enumerate(
        "surface present modes",
        vkGetPhysicalDeviceSurfacePresentModesKHR,
        physicalDevice.get(),
        _handle.get()
    );
}

vector<VkSurfaceFormatKHR> Surface::formats(PhysicalDevice const& physicalDevice) const {
    debug_check(*this);

    return dev::enumerate(
        "surface formats",
        vkGetPhysicalDeviceSurfaceFormatsKHR,
        physicalDevice.get(),
        _handle.get()
    );
}

VkSurfaceCapabilitiesKHR Surface::capabilities(PhysicalDevice const& physicalDevice) const {
    debug_check(*this);

    VkSurfaceCapabilitiesKHR capabilities{};
    auto const result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        physicalDevice.get(),
        _handle.get(),
        &capabilities
    );
    CVK_RESULT_STABLE_THROW(
        result != VK_SUCCESS,
        result,
        "device-surface capabilities query failed"
    ) {}

    return capabilities;
}

VkExtent2D Surface::extent(PhysicalDevice const& physicalDevice) const {
    auto const capabilities = this->capabilities(physicalDevice);
    if(capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
        return capabilities.currentExtent;

    CTH_CRITICAL(!_extentCallback, "a surface without a fixed extent needs an extent callback") {}
    auto const extent = _extentCallback();
    if(extent.width == 0 || extent.height == 0)
        return VkExtent2D{};
    return Swapchain::evalExtent(extent, capabilities);
}

void destroy(vk_not_null<VkInstance> vkInstance, VkSurfaceKHR vkSurface) {
    CTH_WARN(vkSurface == VK_NULL_HANDLE, "vkSurface invalid (VK_NULL_HANDLE)") {}

    vkDestroySurfaceKHR(vkInstance.get(), vkSurface, nullptr);

    log::msg<except::LOG>("destroyed surface");
}

void Surface::forget() { _extentCallback = {}; }
}
