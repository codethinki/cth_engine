module;
#include "lib/volk.hpp"
export module cth.vk.constants.debug;

import cth.constants;


export namespace cth::vk::constants {
constexpr bool DEBUG_MODE = COMPILATION_MODE == CompilationMode::DEBUG;
constexpr VkDebugUtilsMessageSeverityFlagsEXT DEBUG_MESSAGE_SEVERITY = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
    VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
constexpr VkDebugUtilsMessageTypeFlagsEXT DEBUG_MESSAGE_TYPE = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
    | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
    | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
} //namespace cth::vk::constants
