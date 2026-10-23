#pragma once
#include "cvk/base/queue/queue_family.hpp"

#include <volk.h>

#include <cth/io/log.hpp>

#include <algorithm>
#include <vector>


namespace cvk {
/**
 * @brief allowed formats and present modes, image count, usage and sharing of a swapchain
 * @details @ref formats and @ref presentModes are preferences, one entry -> exactly this entry or a throw
 */
struct SwapchainConfig {
    /**
     * @brief allowed formats, the first one the surface offers wins
     * @details empty -> the first format of the surface
     */
    std::vector<VkSurfaceFormatKHR> formats{
        {VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
        {VK_FORMAT_R8G8B8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
    };

    /**
     * @brief allowed present modes, the first one the surface offers wins
     * @details empty -> VK_PRESENT_MODE_FIFO_KHR (always available)
     */
    std::vector<VkPresentModeKHR> presentModes{VK_PRESENT_MODE_FIFO_KHR};

    /**
     * @brief 0 -> minImageCount of the surface + 1, clamped to the limits of the surface
     */
    uint32_t minImageCount = 0;

    /**
     * @pre != 0
     */
    VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    /**
     * @brief queue families that use the images at the same time
     * @details size < 2 -> VK_SHARING_MODE_EXCLUSIVE, else VK_SHARING_MODE_CONCURRENT
     * @pre unique
     */
    std::vector<queue_family_index_t> queueFamilies{};

    VkCompositeAlphaFlagBitsKHR compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

    /**
     * @attention pQueueFamilyIndices points into @ref queueFamilies
     */
    [[nodiscard]] constexpr VkSwapchainCreateInfoKHR createInfo(
        VkSurfaceKHR surface,
        VkSurfaceFormatKHR format,
        VkPresentModeKHR presentMode,
        VkSurfaceTransformFlagBitsKHR transform,
        VkExtent2D extent,
        uint32_t imageCount,
        VkSwapchainKHR oldSwapchain
    ) const {
        return VkSwapchainCreateInfoKHR{
            .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            .pNext = nullptr,
            .flags = 0,
            .surface = surface,
            .minImageCount = imageCount,
            .imageFormat = format.format,
            .imageColorSpace = format.colorSpace,
            .imageExtent = extent,
            .imageArrayLayers = 1,
            .imageUsage = usage,
            .imageSharingMode = queueFamilies.size() >= 2 ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = static_cast<uint32_t>(queueFamilies.size()),
            .pQueueFamilyIndices = queueFamilies.data(),
            .preTransform = transform,
            .compositeAlpha = compositeAlpha,
            .presentMode = presentMode,
            .clipped = VK_TRUE,
            .oldSwapchain = oldSwapchain,
        };
    }

    static constexpr void debug_check(SwapchainConfig const&);
};
}

namespace cvk {
constexpr void SwapchainConfig::debug_check(SwapchainConfig const& config) {
    CTH_CRITICAL(config.usage == 0, "usage must not be 0") {}

    auto families = config.queueFamilies;
    std::ranges::sort(families);
    CTH_CRITICAL(std::ranges::adjacent_find(families) != families.end(), "queue families must be unique") {}
}
}
