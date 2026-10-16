// ReSharper disable CppClangTidyCertDcl58Cpp
#pragma once
#include <volk.h>
#include <vulkan/vk_enum_string_helper.h>

#include <cth/string/format.hpp>

#include <string_view>


CTH_FORMAT_ALIAS(VkResult, std::string_view, string_VkResult);

CTH_FORMAT_ALIAS(VkFormat, std::string_view, string_VkFormat);

CTH_FORMAT_ALIAS(VkStructureType, std::string_view, string_VkStructureType);

CTH_FORMAT_ALIAS(VkDescriptorType, std::string_view, string_VkDescriptorType);

CTH_FORMAT_ALIAS(VkColorSpaceKHR, std::string_view, string_VkColorSpaceKHR);

CTH_FORMAT_ALIAS(VkPresentModeKHR, std::string_view, string_VkPresentModeKHR);

CTH_FORMAT_ALIAS(VkObjectType, std::string_view, string_VkObjectType);
