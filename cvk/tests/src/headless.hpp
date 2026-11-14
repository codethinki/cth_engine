#pragma once
#include "cvk/base/instance.hpp"
#include "cvk/base/physical_device.hpp"
#include "cvk/surface/surface.hpp"

#include <cth/ptr/ref_wrapper.hpp>

#include <initializer_list>
#include <span>
#include <string>
#include <vector>


namespace cvk::test {
/**
 * instance extensions for headless surfaces with present support
 */
[[nodiscard]] std::vector<std::string> headless_extensions();

/**
 * window extent of `headless_surface()` without an own callback
 */
inline constexpr VkExtent2D HEADLESS_EXTENT{64, 64};

/**
 * creates a headless surface
 * @param extentCallback for its undefined `currentExtent`
 * @pre the instance enables `headless_extensions()`
 */
[[nodiscard]] Surface headless_surface(
    Instance const&,
    Surface::extent_callback_t extentCallback = [] { return HEADLESS_EXTENT; }
);

/**
 * `pick_physical_device()` with `constants::BASE_DEVICE_FEATURES`
 * @details a present queue adds `constants::PRESENT_DEVICE_FEATURES` and `constants::PRESENT_DEVICE_EXTENSIONS`
 * @throws std::bad_optional_access no device meets the requirements
 */
[[nodiscard]] PhysicalDevicePick pick(
    Instance const&,
    std::span<QueueFamilyProperties const> queueProperties,
    std::initializer_list<cth::ref_wrapper<Surface const>> surfaces = {}
);
}
