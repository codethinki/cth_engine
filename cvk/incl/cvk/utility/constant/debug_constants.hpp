#pragma once
#include <volk.h>

#include <cth/constants.hpp>

namespace cvk::constants {
/**
 * @brief true in debug builds, follows @ref cth::debug_mode()
 */
constexpr bool DEBUG_MODE = cth::debug_mode();

constexpr VkDebugUtilsMessageSeverityFlagsEXT DEBUG_MESSAGE_SEVERITY =
    VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
    | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

constexpr VkDebugUtilsMessageTypeFlagsEXT DEBUG_MESSAGE_TYPE =
    VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
    | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
    | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
} // namespace cvk::constants
