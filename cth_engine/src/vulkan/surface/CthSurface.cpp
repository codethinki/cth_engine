#include "CthSurface.hpp"

#include "CthOSWindow.hpp"
#include "vulkan/base/CthPhysicalDevice.hpp"
#include "vulkan/resource/CthDestructionQueue.hpp"
#include "vulkan/utility/cth_vk_utils.hpp"



namespace cth::vk {
using std::vector;

Surface::Surface(not_null<Instance const*> instance, DestructionQueue* destruction_queue, State const& state) : Surface{instance, destruction_queue} {
    wrap(state);
}

Surface::~Surface() {
    optDestroy();

    log::msg("destroyed surface");
}
void Surface::wrap(State const& state) {
    DEBUG_CHECK_SURFACE_HANDLE(state.vkSurface);
    optDestroy();

    _handle = state.vkSurface.get();
}
void Surface::destroy() {

    if(_destructionQueue) _destructionQueue->push(_handle.get());
    else destroy(_instance->get(), _handle.get());

    _handle = VK_NULL_HANDLE;
}
Surface::State Surface::release() {
    DEBUG_CHECK_SURFACE(this);
    State const state{_handle.get()};

    reset();
    return state;
}

bool Surface::supportsFamily(PhysicalDevice const& physical_device, uint32_t family_index) const {
    VkBool32 support = false;
    VkResult const result = vkGetPhysicalDeviceSurfaceSupportKHR(physical_device.get(), family_index, _handle.get(), &support);
    CTH_STABLE_ERR(result != VK_SUCCESS, "device-surface support query failed")
        throw vk::result_exception{result, details->exception()};
    return support;
}
vector<VkPresentModeKHR> Surface::presentModes(PhysicalDevice const& physical_device) const {
    uint32_t size = 0;
    VkResult const result1 = vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device.get(), _handle.get(), &size, nullptr);

    CTH_STABLE_ERR(result1 != VK_SUCCESS, "device-surface present modes query failed")
        throw vk::result_exception{result1, details->exception()};

    if(!size) return {};

    vector<VkPresentModeKHR> modes(size);
    VkResult const result2 = vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device.get(), _handle.get(), &size, modes.data());

    CTH_STABLE_ERR(result2 != VK_SUCCESS, "device-surface present modes query failed")
        throw vk::result_exception{result2, details->exception()};

    return modes;
}
vector<VkSurfaceFormatKHR> Surface::formats(PhysicalDevice const& physical_device) const {
    uint32_t size = 0;
    VkResult const result1 = vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device.get(), _handle.get(), &size, nullptr);

    CTH_STABLE_ERR(result1 != VK_SUCCESS, "device-surface formats query failed")
        throw vk::result_exception{result1, details->exception()};

    if(!size) return {};

    vector<VkSurfaceFormatKHR> formats(size);

    VkResult const result2 = vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device.get(), _handle.get(), &size, formats.data());
    CTH_STABLE_ERR(result2 != VK_SUCCESS, "device-surface formats query failed")
        throw vk::result_exception{result2, details->exception()};

    return formats;
}

VkSurfaceCapabilitiesKHR Surface::capabilities(PhysicalDevice const& physical_device) const {
    VkSurfaceCapabilitiesKHR capabilities;
    auto const result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device.get(), _handle.get(), &capabilities);

    CTH_STABLE_ERR(result != VK_SUCCESS, "device-surface capabilities query failed")
        throw vk::result_exception{result, details->exception()};

    return capabilities;
}
Surface Surface::Temp(not_null<Instance const*> instance, DestructionQueue* destruction_queue) {
    return Surface{instance, destruction_queue, State{OSWindow::tempSurface(instance)}};
}
void Surface::destroy(vk::not_null<VkInstance> instance, VkSurfaceKHR surface) {
    CTH_WARN(surface == VK_NULL_HANDLE, "surface invalid (VK_NULL_HANDLE)") {}
    DEBUG_CHECK_INSTANCE_HANDLE(instance);

    vkDestroySurfaceKHR(instance.get(), surface, nullptr);
}
void Surface::reset() { _handle = VK_NULL_HANDLE; }

void Surface::debug_check(not_null<Surface const*> surface) { DEBUG_CHECK_SURFACE_HANDLE(surface->get()); }
void Surface::debug_check_handle([[maybe_unused]] vk::not_null<VkSurfaceKHR> surface) {}



}
