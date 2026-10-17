#pragma once
#include <volk.h>


namespace cvk {
/**
 * @brief mip range and aspect of an image view
 */
struct ImageViewConfig {
    uint32_t baseMipLevel = 0;

    /**
     * @brief VK_REMAINING_MIP_LEVELS -> to the last mip level of the image
     */
    uint32_t mipLevels = VK_REMAINING_MIP_LEVELS;

    /**
     * @brief 0 -> the aspect mask of the image
     */
    VkImageAspectFlags aspectMask = 0;

    /**
     * @param imageAspectMask aspect mask of the image
     */
    [[nodiscard]] constexpr VkImageSubresourceRange range(VkImageAspectFlags imageAspectMask) const {
        return VkImageSubresourceRange{
            .aspectMask = aspectMask != 0 ? aspectMask : imageAspectMask,
            .baseMipLevel = baseMipLevel,
            .levelCount = mipLevels,
            .baseArrayLayer = 0,
            .layerCount = 1,
        };
    }
};
}
