#pragma once
namespace jvk::fmt {
[[nodiscard]] constexpr std::string_view to_string(VkResult result);
[[nodiscard]] constexpr std::string_view to_string(VkFormat format);

[[nodiscard]] constexpr std::string_view to_string(VkStructureType structure);
[[nodiscard]] constexpr std::string_view to_string(VkDescriptorType type);

[[nodiscard]] constexpr std::string_view to_string(VkColorSpaceKHR color_space);
[[nodiscard]] constexpr std::string_view to_string(VkPresentModeKHR format);
}

#include "vk_to_string.inl"
