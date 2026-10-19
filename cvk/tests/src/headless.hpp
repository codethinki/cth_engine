#pragma once
#include "cvk/base/instance.hpp"
#include "cvk/base/physical_device.hpp"
#include "cvk/surface/surface.hpp"

#include <span>
#include <string>
#include <vector>


namespace cvk::test {
/**
 * @brief instance extensions for headless surfaces with present support
 */
[[nodiscard]] std::vector<std::string> headless_extensions();

/**
 * @brief creates a headless surface
 * @pre the instance enables @ref headless_extensions()
 */
[[nodiscard]] Surface headless_surface(Instance const&, Surface::Config = {});

/**
 * @brief @ref pick_physical_device() with @ref constants::BASE_DEVICE_FEATURES
 * @details a present queue adds @ref constants::PRESENT_DEVICE_FEATURES and @ref constants::PRESENT_DEVICE_EXTENSIONS
 * @throws std::bad_optional_access no device meets the requirements
 */
[[nodiscard]] PhysicalDevicePick pick(
    Instance const&,
    std::span<QueueFamilyProperties const> queueProperties,
    std::span<Surface const> surfaces = {}
);
}
