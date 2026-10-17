#pragma once
#include "image_format.hpp"

#include "cvk/base/queue/queue_family.hpp"
#include "cvk/data/memory/memory_config.hpp"

#include <volk.h>

#include <cth/io/log.hpp>

#include <algorithm>
#include <vector>


namespace cvk {
/**
 * @brief 2D image: format, usage, mip levels, who uses it and where its memory lives
 */
struct ImageConfig {
    /**
     * @brief 0 -> the aspects of @ref format, see @ref format_aspects()
     * @pre the aspects of @ref format contain it
     */
    VkImageAspectFlags aspectMask = 0;

    /**
     * @pre != VK_FORMAT_UNDEFINED
     */
    VkFormat format = VK_FORMAT_UNDEFINED;

    /**
     * @pre != 0
     */
    VkImageUsageFlags usage = 0;

    /**
     * @brief cube compatible, mutable format, sparse, ...
     */
    VkImageCreateFlags flags = 0;

    VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL;

    /**
     * @pre >= 1
     */
    uint32_t mipLevels = 1;

    VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;

    /**
     * @pre VK_IMAGE_LAYOUT_UNDEFINED or VK_IMAGE_LAYOUT_PREINITIALIZED
     */
    VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    /**
     * @brief queue families that use the image at the same time
     * @details size < 2 -> VK_SHARING_MODE_EXCLUSIVE, else VK_SHARING_MODE_CONCURRENT
     * @pre unique
     */
    std::vector<queue_family_index_t> queueFamilies{};

    /**
     * @brief memory of the image
     */
    MemoryConfig memory{};

    /**
     * @brief depth attachment, depth aspect only
     */
    [[nodiscard]] static constexpr ImageConfig DepthBuffer(VkFormat depthFormat, VkSampleCountFlagBits samples) {
        return ImageConfig{
            .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
            .format = depthFormat,
            .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            .samples = samples,
        };
    }

    /**
     * @attention pQueueFamilyIndices points into @ref queueFamilies
     */
    [[nodiscard]] constexpr VkImageCreateInfo createInfo(VkExtent2D extent) const {
        return VkImageCreateInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = flags,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = format,
            .extent = {extent.width, extent.height, 1},
            .mipLevels = mipLevels,
            .arrayLayers = 1,
            .samples = samples,
            .tiling = tiling,
            .usage = usage,
            .sharingMode = queueFamilies.size() >= 2 ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = static_cast<uint32_t>(queueFamilies.size()),
            .pQueueFamilyIndices = queueFamilies.data(),
            .initialLayout = initialLayout,
        };
    }

    static constexpr void debug_check(ImageConfig const&);
};
}

namespace cvk {
constexpr void ImageConfig::debug_check(ImageConfig const& config) {
    CTH_CRITICAL(config.format == VK_FORMAT_UNDEFINED, "format must not be undefined") {}
    CTH_CRITICAL(
        (config.aspectMask & ~format_aspects(config.format)) != 0,
        "the aspects of the format must contain the aspect mask"
    ) {}
    CTH_CRITICAL(config.usage == 0, "usage must not be 0") {}
    CTH_CRITICAL(config.mipLevels == 0, "at least one mip level is required") {}
    CTH_CRITICAL(
        config.initialLayout != VK_IMAGE_LAYOUT_UNDEFINED && config.initialLayout != VK_IMAGE_LAYOUT_PREINITIALIZED,
        "initial layout must be undefined or preinitialized"
    ) {}

    auto families = config.queueFamilies;
    std::ranges::sort(families);
    CTH_CRITICAL(std::ranges::adjacent_find(families) != families.end(), "queue families must be unique") {}
}
}
