#include "CthSurface.hpp"

#include "CthOSWindow.hpp"
#include "src/vulkan/base/CthPhysicalDevice.hpp"
#include "src/vulkan/resource/CthDestructionQueue.hpp"
#include "src/vulkan/utility/cth_vk_exceptions.hpp"
#include "src/vulkan/utility/cth_vk_overloads.hpp"


namespace cth::vk {
using std::vector;

Surface::Surface(Instance const& instance, DestructionQueue* destruction_queue, Config config, State const& state) : Surface{instance,
    destruction_queue, std::move(config)} { wrap(state); }

Surface::~Surface() {
    optDestroy();

    log::msg("destroyed surface");
}
void Surface::wrap(State const& state) {
    Surface::debug_check_handle(state.vkSurface);
    optDestroy();

    _handle = state.vkSurface.get();
}
void Surface::destroy() {
    Surface::debug_check(*this);

    auto const lambda = [vk_instance = _instance->get(), vk_surface = _handle.get()]() { destroy(vk_instance, vk_surface); };


    if(_destructionQueue) _destructionQueue->push(lambda);
    else lambda();

    reset();
}
Surface::State Surface::release() {
    Surface::debug_check(*this);
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
auto Surface::presentModes(PhysicalDevice const& physical_device) const -> vector<present_mode_t> {
    auto const& allowed = _config.allowedPresentModes;

    uint32_t size = 0;
    VkResult const result1 = vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device.get(), _handle.get(), &size, nullptr);

    CTH_STABLE_ERR(result1 != VK_SUCCESS, "device-surface present modes query failed")
        throw vk::result_exception{result1, details->exception()};

    if(!size) return {};

    vector<present_mode_t> modes(size);
    VkResult const result2 = vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device.get(), _handle.get(), &size, modes.data());

    CTH_STABLE_ERR(result2 != VK_SUCCESS, "device-surface present modes query failed")
        throw vk::result_exception{result2, details->exception()};

    if(!allowed.empty()) std::erase_if(modes, [&allowed](present_mode_t mode) { return !std::ranges::contains(allowed, mode); });


    return modes;
}
auto Surface::formats(PhysicalDevice const& physical_device) const -> vector<format_t> {
    auto const& allowed = _config.allowedSurfaceFormats;

    uint32_t size = 0;
    VkResult const result1 = vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device.get(), _handle.get(), &size, nullptr);

    CTH_STABLE_ERR(result1 != VK_SUCCESS, "device-surface formats query failed")
        throw vk::result_exception{result1, details->exception()};

    if(!size) return {};

    vector<format_t> formats(size);

    VkResult const result2 = vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device.get(), _handle.get(), &size, formats.data());
    CTH_STABLE_ERR(result2 != VK_SUCCESS, "device-surface formats query failed")
        throw vk::result_exception{result2, details->exception()};

    if(!allowed.empty()) std::erase_if(formats, [&allowed](format_t format) { return !std::ranges::contains(allowed, format); });

    return formats;
}
auto Surface::format(PhysicalDevice const& physical_device) const -> format_t {
    auto const availableFormats = formats(physical_device);
    CTH_STABLE_ERR(availableFormats.empty(), "no suitable format found") {
        details->add("available: {}", availableFormats);
        details->add("allowed: {}", _config.allowedSurfaceFormats);
        throw details->exception();
    }
    return availableFormats[0];
}

VkSurfaceCapabilitiesKHR Surface::capabilities(PhysicalDevice const& physical_device) const {
    VkSurfaceCapabilitiesKHR capabilities;
    auto const result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device.get(), _handle.get(), &capabilities);

    CTH_STABLE_ERR(result != VK_SUCCESS, "device-surface capabilities query failed")
        throw vk::result_exception{result, details->exception()};

    return capabilities;
}
auto Surface::presentMode(PhysicalDevice const& physical_device) const -> present_mode_t {
    auto const availableModes = presentModes(physical_device);
    CTH_STABLE_ERR(availableModes.empty(), "no suitable present mode found") {
        details->add("available: {}", availableModes);
        details->add("allowed: {}", _config.allowedPresentModes);
        throw details->exception();
    }
    return availableModes[0];
}

Surface Surface::Temp(Instance const& instance, DestructionQueue* destruction_queue) {
    return Surface{instance, destruction_queue, {}, State{OSWindow::tempSurface(instance)}};
}
void Surface::destroy(vk::not_null<VkInstance> instance, VkSurfaceKHR surface) {
    CTH_WARN(surface == VK_NULL_HANDLE, "surface invalid (VK_NULL_HANDLE)") {}
    debug_check_handle(surface);

    vkDestroySurfaceKHR(instance.get(), surface, nullptr);
}
void Surface::reset() { _handle = VK_NULL_HANDLE; }

}
