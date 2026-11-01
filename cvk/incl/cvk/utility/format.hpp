#pragma once
// ReSharper disable CppClangTidyCertDcl58Cpp
#include <volk.h>
#include <vk_mem_alloc.h>
#include <vulkan/vk_enum_string_helper.h>

#include <cth/meta/concepts.hpp>
#include <cth/string/format.hpp>

#include <string_view>
#include <tuple>


namespace cvk {
/**
 * T is a Vulkan or VMA handle that cvk uses
 */
template<class T>
concept vk_handle = cth::mta::is_any_of<
    T,
    VkInstance,
    VkPhysicalDevice,
    VkDevice,
    VkQueue,
    VkCommandBuffer,
    VkSemaphore,
    VkFence,
    VkImage,
    VkImageView,
    VkSampler,
    VkBuffer,
    VkDescriptorPool,
    VkDescriptorSet,
    VkDescriptorSetLayout,
    VkPipelineLayout,
    VkPipeline,
    VkShaderModule,
    VkCommandPool,
    VkSwapchainKHR,
    VkSurfaceKHR,
    VkDebugUtilsMessengerEXT,
    VmaAllocation,
    VmaAllocator>;
}

/**
 * formats the handle as its address
 */
CTH_FORMAT_CPT(cvk::vk_handle, [](auto handle) { return static_cast<void const*>(handle); });

CTH_FORMAT_ALIAS(VkResult, std::string_view, string_VkResult);

CTH_FORMAT_ALIAS(VkFormat, std::string_view, string_VkFormat);

CTH_FORMAT_ALIAS(VkStructureType, std::string_view, string_VkStructureType);

CTH_FORMAT_ALIAS(VkDescriptorType, std::string_view, string_VkDescriptorType);

CTH_FORMAT_ALIAS(VkColorSpaceKHR, std::string_view, string_VkColorSpaceKHR);

CTH_FORMAT_ALIAS(VkPresentModeKHR, std::string_view, string_VkPresentModeKHR);

CTH_FORMAT_ALIAS(VkObjectType, std::string_view, string_VkObjectType);

CTH_FORMAT_ALIAS(VkImageLayout, std::string_view, string_VkImageLayout);

CTH_FORMAT_CLASS(
    VkSurfaceFormatKHR,
    "{}, {}",
    [](VkSurfaceFormatKHR const& f) { return std::tie(f.format, f.colorSpace); }
);

CTH_FORMAT_CLASS(VkExtent2D, "{}, {}", [](VkExtent2D const& e) { return std::tie(e.width, e.height); });
