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
 * @pre @ref instance enables @ref headless_extensions()
 */
[[nodiscard]] Surface headless_surface(Instance const& instance, Surface::Config config = {});

/**
 * @brief @ref PhysicalDevice::AutoPick() for tests
 * @throws std::bad_optional_access no device meets the requirements
 */
[[nodiscard]] PhysicalDevice::CreateResult pick(
    Instance const& instance,
    std::span<QueueFamilyProperties const> queueProperties,
    std::span<Surface const> surfaces = {}
);
}
