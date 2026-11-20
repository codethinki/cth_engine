#pragma once
#include "image.hpp"

#include <volk.h>

#include <cth/io/log.hpp>


namespace cvk {
/**
 * config of an `ImageView`
 * @details the input of an image descriptor too, see `SampledImageDescriptor`
 */
struct ImageViewConfig {
    /**
     * `VK_IMAGE_VIEW_TYPE_MAX_ENUM` -> inferred
     * @see `resolvedViewType()`
     */
    VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_MAX_ENUM;

    uint32_t baseMipLevel = 0;

    /** `VK_REMAINING_MIP_LEVELS` -> to the last mip level of the image */
    uint32_t mipLevels = VK_REMAINING_MIP_LEVELS;

    VkImageAspectFlags aspectMask = 0; ///< `0` -> the aspect mask of the image

    uint32_t baseArrayLayer = 0;

    /** `VK_REMAINING_ARRAY_LAYERS` -> to the last layer of the image */
    uint32_t arrayLayers = VK_REMAINING_ARRAY_LAYERS;

    /**
     * @param imageAspectMask aspect mask of the image
     */
    [[nodiscard]] constexpr VkImageSubresourceRange
    range(VkImageAspectFlags imageAspectMask) const {
        return VkImageSubresourceRange{
            .aspectMask = aspectMask != 0 ? aspectMask : imageAspectMask,
            .baseMipLevel = baseMipLevel,
            .levelCount = mipLevels,
            .baseArrayLayer = baseArrayLayer,
            .layerCount = arrayLayers,
        };
    }

    /**
     * @return the layers of the view of the image
     */
    [[nodiscard]] constexpr uint32_t layerCount(Image const& image) const {
        return arrayLayers == VK_REMAINING_ARRAY_LAYERS
            ? image.arrayLayers() - baseArrayLayer
            : arrayLayers;
    }

    /**
     * @return `viewType`, `VK_IMAGE_VIEW_TYPE_MAX_ENUM` -> inferred for the image
     * @details inferred:
     * - one layer -> 2D
     * - cube compatible image with 6 layers -> cube, a multiple of 6 -> cube array
     * - else 2D array
     */
    [[nodiscard]] constexpr VkImageViewType resolvedViewType(Image const& image) const {
        if(viewType != VK_IMAGE_VIEW_TYPE_MAX_ENUM)
            return viewType;

        auto const layers = layerCount(image);
        if(layers == 1)
            return VK_IMAGE_VIEW_TYPE_2D;
        if((image.config().flags & VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT) == 0 || layers % 6 != 0)
            return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        return layers == 6 ? VK_IMAGE_VIEW_TYPE_CUBE : VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
    }

    /**
     * @return create info of the view of the image, identity swizzle
     * @pre `debug_check()` passes for the image
     */
    [[nodiscard]] constexpr VkImageViewCreateInfo createInfo(Image const& image) const {
        return VkImageViewCreateInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .image = image.get(),
            .viewType = resolvedViewType(image),
            .format = image.format(),
            .components = {
                .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                .a = VK_COMPONENT_SWIZZLE_IDENTITY,
            },
            .subresourceRange = range(image.aspectMask()),
        };
    }

    /**
     * checks the config for the image
     * @details the image is created, the mip range and the layer range lie in the image
     */
    static constexpr void debug_check(ImageViewConfig const&, Image const&);
};
}

namespace cvk {
constexpr void ImageViewConfig::debug_check(ImageViewConfig const& config, Image const& image) {
    Image::debug_check(image);
    CTH_CRITICAL(
        config.baseMipLevel >= image.mipLevels(),
        "base mip level must lie in the image, got {} of {}",
        config.baseMipLevel,
        image.mipLevels()
    ) {}
    CTH_CRITICAL(
        config.mipLevels != VK_REMAINING_MIP_LEVELS && config.mipLevels > image.mipLevels() - config
        .
        baseMipLevel,
        "mip levels must lie in the image, got {} + {} of {}",
        config.baseMipLevel,
        config.mipLevels,
        image.mipLevels()
    ) {}
    CTH_CRITICAL(
        config.baseArrayLayer >= image.arrayLayers(),
        "base array layer must lie in the image, got {} of {}",
        config.baseArrayLayer,
        image.arrayLayers()
    ) {}
    CTH_CRITICAL(
        config.arrayLayers != VK_REMAINING_ARRAY_LAYERS
        && config.arrayLayers > image.arrayLayers() - config.baseArrayLayer,
        "array layers must lie in the image, got {} + {} of {}",
        config.baseArrayLayer,
        config.arrayLayers,
        image.arrayLayers()
    ) {}
}
}
