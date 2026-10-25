#pragma once
#include "jvk/cmd/cmd_buffer.hpp"

#include "cvk/data/img/image.hpp"

#include <volk.h>

#include <cstddef>
#include <cstdint>


namespace cvk {
class BaseBuffer;
}

namespace jvk {
/**
 * cvk image whose commands record their accesses in the resource states of the command buffer
 * @details the commands take a jvk command buffer and record the barriers that their accesses need,
 *  the untracked commands of @ref cvk::Image stay reachable through cvk::Image::
 */
class Image : public cvk::Image {
public:
    using cvk::Image::Image;

    /**
     * records a copy from the buffer into every layer of the mip level, with its access
     * @param srcOffset in bytes, the layers follow each other in the buffer
     * @param layout of the mip level, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL or VK_IMAGE_LAYOUT_GENERAL
     * @details accesses: the mip level in the layout, transfer stage, transfer write,
     *  the buffer: transfer stage, transfer read
     * @pre the preconditions of @ref cvk::Image::copy()
     */
    void copy(
        cmd_buffer auto&,
        cvk::BaseBuffer const& src,
        size_t srcOffset = 0,
        uint32_t mipLevel = 0,
        VkImageLayout layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
    ) const;

    /**
     * records a copy from every layer of the mip level into the buffer, with its access
     * @param dstOffset in bytes, the layers follow each other in the buffer
     * @param layout of the mip level, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL or VK_IMAGE_LAYOUT_GENERAL
     * @details accesses: the mip level in the layout, transfer stage, transfer read,
     *  the buffer: transfer stage, transfer write
     * @pre the preconditions of @ref cvk::Image::copyTo()
     */
    void copyTo(
        cmd_buffer auto&,
        cvk::BaseBuffer const& dst,
        size_t dstOffset = 0,
        uint32_t mipLevel = 0,
        VkImageLayout layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
    ) const;

    /**
     * records blits that fill each mip level from the level before it, every layer, with their accesses
     * @param firstMipLevel first level to fill, the blits read from firstMipLevel - 1
     * @param mipLevels (VK_REMAINING_MIP_LEVELS => to the last mip level)
     * @param filter depth formats need VK_FILTER_NEAREST
     * @details accesses per blit: the source level in VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, transfer read,
     *  the filled level in VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, transfer write
     * @pre the preconditions of @ref cvk::Image::blitMipLevels() without the layouts
     * @post the last state of the levels from firstMipLevel - 1 to the last filled level:
     *  VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, transfer read
     */
    void blitMipLevels(
        cmd_buffer auto&,
        uint32_t firstMipLevel = 1,
        uint32_t mipLevels = VK_REMAINING_MIP_LEVELS,
        VkFilter filter = VK_FILTER_LINEAR
    ) const;
};
}
