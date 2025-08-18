#pragma once
namespace cth::vk::fmt {
[[nodiscard]] constexpr std::string_view to_string(VkResult result);
[[nodiscard]] constexpr std::string_view to_string(VkFormat format);

[[nodiscard]] constexpr std::string_view to_string(VkStructureType structure);
[[nodiscard]] constexpr std::string_view to_string(VkDescriptorType type);

[[nodiscard]] constexpr std::string_view to_string(VkColorSpaceKHR color_space);
[[nodiscard]] constexpr std::string_view to_string(VkPresentModeKHR format);
}

#include "cth_vk_to_string.inl"