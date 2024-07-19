#pragma once
#ifndef _FINAL
#define CONSTANT_DEBUG_MODE
#endif

namespace cth::vk::constants {
static constexpr bool DEBUG_MODE =
#ifdef CONSTANT_DEBUG_MODE
    true;
#else
    false;
#endif
static constexpr VkDebugUtilsMessageSeverityFlagsEXT DEBUG_MESSAGE_SEVERITY = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
    VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
static constexpr VkDebugUtilsMessageTypeFlagsEXT DEBUG_MESSAGE_TYPE = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
    VK_DEBUG_UTILS_MESSAGE_TYPE_DEVICE_ADDRESS_BINDING_BIT_EXT |
    VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
    VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
} //namespace cth::vk::constants
