#pragma once
#include "flat_hash_map.hpp"

#include <volk.h>

#include <cstdint>
#include <span>
#include <vector>


namespace cvk {
class Image;

/**
 * @brief layout of each mip level of the images, for the functions that transition them
 * @details the caller owns it and decides the scope: one command buffer, one thread or one serial frame,
 *  unknown image -> every mip level is VK_IMAGE_LAYOUT_UNDEFINED
 * @attention follows the order of the commands, use it on one thread
 * @attention keeps the entry of a destroyed image and a new image can get the same handle,
 *  call @ref erase() before the image is destroyed
 */
class ImageLayouts {
public:
    /**
     * @return the layout of the mip level, unknown image -> VK_IMAGE_LAYOUT_UNDEFINED
     * @pre mipLevel < @ref Image::mipLevels()
     */
    [[nodiscard]] VkImageLayout layout(Image const&, uint32_t mipLevel = 0) const;

    /**
     * @return the layout of each mip level, unknown image -> empty
     */
    [[nodiscard]] std::span<VkImageLayout const> layouts(Image const&) const;

    /**
     * @brief sets the layout of the mip levels
     * @param mipLevels (VK_REMAINING_MIP_LEVELS => to the last mip level)
     * @details unknown image -> the other mip levels are VK_IMAGE_LAYOUT_UNDEFINED
     * @pre @ref Image::created()
     * @pre the mip range lies in the image
     */
    void set(Image const&, VkImageLayout, uint32_t firstMipLevel = 0, uint32_t mipLevels = VK_REMAINING_MIP_LEVELS);

    /**
     * @brief removes the image, its layouts are unknown after
     */
    void erase(Image const&);

    /**
     * @brief removes every image
     */
    void clear() { _layouts.clear(); }

    [[nodiscard]] bool contains(Image const&) const;
    [[nodiscard]] size_t size() const { return _layouts.size(); }
    [[nodiscard]] bool empty() const { return _layouts.empty(); }

private:
    //IMPLEMENT key (image, mip level) -> layout: no vector per image, no mip count check
    flat_hash_map<VkImage, std::vector<VkImageLayout>> _layouts{};
};
}
