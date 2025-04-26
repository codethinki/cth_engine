module;
#include "cth/macro.hpp"

#include "lib/volk.hpp"

export module cth.vk.fmt.to_string;

import std;

export namespace cth::vk::fmt {
[[nodiscard]] cxpr std::string_view to_string(VkResult result);
[[nodiscard]] cxpr std::string_view to_string(VkFormat format);

[[nodiscard]] cxpr std::string_view to_string(VkStructureType structure);
[[nodiscard]] cxpr std::string_view to_string(VkDescriptorType type);

[[nodiscard]] cxpr std::string_view to_string(VkColorSpaceKHR color_space);
[[nodiscard]] cxpr std::string_view to_string(VkPresentModeKHR format);
[[nodiscard]] cxpr std::string_view to_string(VkObjectType type);
}

#include "cth_vk_to_string.inl"
