#include "cvk/surface/surface.hpp"

#include "cvk/base/instance.hpp"
#include "cvk/base/physical_device.hpp"
#include "cvk/utility/vk_exceptions.hpp"
#include "cvk/utility/vk_overloads.hpp"

#include <algorithm>


namespace cvk {
using std::vector;

namespace {
    /**
     * @return @ref allowed in its order, filtered by @ref available, empty @ref allowed -> @ref available
     */
    template<class T>
    [[nodiscard]] vector<T> filter_allowed(vector<T> available, std::span<T const> allowed) {
        if(allowed.empty()) return available;

        vector<T> result{};
        for(auto const& value : allowed)
            if(std::ranges::contains(available, value)) result.push_back(value);
        return result;
    }
}

Surface::Surface(Instance const& instance, Config config) : _config{std::move(config)}, _handle{instance} {}

Surface::Surface(Instance const& instance, Config config, State const& state) : Surface{instance, std::move(config)} {
    wrap(state);
}

void Surface::wrap(State const& state) {
    debug_check_handle(state.vkSurface);
    optDestroy();

    _handle.reset(state.vkSurface.get(), _handle.vkInstance());
}

void Surface::destroy() {
    debug_check(*this);

    _handle.reset();

    forget();
}

Surface::State Surface::release() {
    debug_check(*this);
    State const state{_handle.release()};

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

auto Surface::presentModes(PhysicalDevice const& physicalDevice) const -> vector<present_mode_t> {
    debug_check(*this);

    uint32_t size = 0;
    auto const countResult = vkGetPhysicalDeviceSurfacePresentModesKHR(
        physicalDevice.get(),
        _handle.get(),
        &size,
        nullptr
    );
    CVK_RESULT_STABLE_THROW(countResult != VK_SUCCESS, countResult, "device-surface present modes query failed") {}

    vector<present_mode_t> modes(size);
    auto const writeResult = vkGetPhysicalDeviceSurfacePresentModesKHR(
        physicalDevice.get(),
        _handle.get(),
        &size,
        modes.data()
    );
    CVK_RESULT_STABLE_THROW(writeResult != VK_SUCCESS, writeResult, "device-surface present modes query failed") {}

    return filter_allowed<present_mode_t>(std::move(modes), _config.allowedPresentModes);
}

auto Surface::formats(PhysicalDevice const& physicalDevice) const -> vector<format_t> {
    debug_check(*this);

    uint32_t size = 0;
    auto const countResult = vkGetPhysicalDeviceSurfaceFormatsKHR(
        physicalDevice.get(),
        _handle.get(),
        &size,
        nullptr
    );
    CVK_RESULT_STABLE_THROW(countResult != VK_SUCCESS, countResult, "device-surface formats query failed") {}

    vector<format_t> formats(size);
    auto const writeResult = vkGetPhysicalDeviceSurfaceFormatsKHR(
        physicalDevice.get(),
        _handle.get(),
        &size,
        formats.data()
    );
    CVK_RESULT_STABLE_THROW(writeResult != VK_SUCCESS, writeResult, "device-surface formats query failed") {}

    return filter_allowed<format_t>(std::move(formats), _config.allowedSurfaceFormats);
}

auto Surface::format(PhysicalDevice const& physicalDevice) const -> format_t {
    auto const availableFormats = formats(physicalDevice);
    CTH_STABLE_ERR(availableFormats.empty(), "no suitable format found") {
        details->add("allowed: {}", _config.allowedSurfaceFormats);
        throw details->exception();
    }
    return availableFormats[0];
}

VkSurfaceCapabilitiesKHR Surface::capabilities(PhysicalDevice const& physicalDevice) const {
    debug_check(*this);

    VkSurfaceCapabilitiesKHR capabilities{};
    auto const result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        physicalDevice.get(),
        _handle.get(),
        &capabilities
    );
    CVK_RESULT_STABLE_THROW(result != VK_SUCCESS, result, "device-surface capabilities query failed") {}

    return capabilities;
}

auto Surface::presentMode(PhysicalDevice const& physicalDevice) const -> present_mode_t {
    auto const availableModes = presentModes(physicalDevice);
    CTH_STABLE_ERR(availableModes.empty(), "no suitable present mode found") {
        details->add("allowed: {}", _config.allowedPresentModes);
        throw details->exception();
    }
    return availableModes[0];
}

void Surface::destroy(vk_not_null<VkInstance> vkInstance, VkSurfaceKHR vkSurface) {
    CTH_WARN(vkSurface == VK_NULL_HANDLE, "vkSurface invalid (VK_NULL_HANDLE)") {}

    vkDestroySurfaceKHR(vkInstance.get(), vkSurface, nullptr);

    log::msg<except::LOG>("destroyed surface");
}

void Surface::forget() {}
}
