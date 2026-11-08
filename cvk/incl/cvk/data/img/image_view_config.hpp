#pragma once
#include <volk.h>


namespace cvk {
/**
 * config of an `ImageView`
 */
struct ImageViewConfig {
    /**
     * `VK_IMAGE_VIEW_TYPE_MAX_ENUM` -> inferred, see `ImageView::viewType()`
     */
    VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_MAX_ENUM;

    uint32_t baseMipLevel = 0;

    /**
     * `VK_REMAINING_MIP_LEVELS` -> to the last mip level of the image
     */
    uint32_t mipLevels = VK_REMAINING_MIP_LEVELS;

    /**
     * `0` -> the aspect mask of the image
     */
    VkImageAspectFlags aspectMask = 0;

    uint32_t baseArrayLayer = 0;

    /**
     * `VK_REMAINING_ARRAY_LAYERS` -> to the last layer of the image
     */
    uint32_t arrayLayers = VK_REMAINING_ARRAY_LAYERS;

    /**
     * @param imageAspectMask aspect mask of the image
     */
    [[nodiscard]] constexpr VkImageSubresourceRange range(VkImageAspectFlags imageAspectMask) const {
        return VkImageSubresourceRange{
            .aspectMask = aspectMask != 0 ? aspectMask : imageAspectMask,
            .baseMipLevel = baseMipLevel,
            .levelCount = mipLevels,
            .baseArrayLayer = baseArrayLayer,
            .layerCount = arrayLayers,
        };
    }
};
}
