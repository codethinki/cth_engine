#pragma once
#include "jvk/cmd/cmd_buffer.hpp"
#include "jvk/sync/resource_states.hpp"

#include "cvk/data/buffer/base_buffer.hpp"
#include "cvk/data/img/image.hpp"

#include <volk.h>

#include <cth/io/log.hpp>

#include <cstddef>
#include <cstdint>


namespace jvk {
/**
 * cvk image whose commands record their accesses in the resource states of the command buffer
 * @details the commands take a jvk command buffer and record the barriers that their accesses need,
 *  the untracked commands of @ref cvk::Image stay reachable through cvk::Image::,
 *  the image keeps no state: a created image is in its creation layout, the recordings track it
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
        cmd_buffer auto& cmd,
        cvk::BaseBuffer const& src,
        size_t srcOffset = 0,
        uint32_t mipLevel = 0,
        VkImageLayout layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
    ) const {
        cmd.use(*this, mipAccess(layout, VK_ACCESS_2_TRANSFER_WRITE_BIT, mipLevel, 1));
        cmd.use(src, BufferAccess{.stages = TRANSFER, .access = VK_ACCESS_2_TRANSFER_READ_BIT});
        cvk::Image::copy(cmd, src, srcOffset, mipLevel, layout);
    }

    /**
     * records a copy from every layer of the mip level into the buffer, with its access
     * @param dstOffset in bytes, the layers follow each other in the buffer
     * @param layout of the mip level, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL or VK_IMAGE_LAYOUT_GENERAL
     * @details accesses: the mip level in the layout, transfer stage, transfer read,
     *  the buffer: transfer stage, transfer write
     * @pre the preconditions of @ref cvk::Image::copyTo()
     */
    void copyTo(
        cmd_buffer auto& cmd,
        cvk::BaseBuffer const& dst,
        size_t dstOffset = 0,
        uint32_t mipLevel = 0,
        VkImageLayout layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
    ) const {
        cmd.use(*this, mipAccess(layout, VK_ACCESS_2_TRANSFER_READ_BIT, mipLevel, 1));
        cmd.use(dst, BufferAccess{.stages = TRANSFER, .access = VK_ACCESS_2_TRANSFER_WRITE_BIT});
        cvk::Image::copyTo(cmd, dst, dstOffset, mipLevel, layout);
    }

    /**
     * records blits that fill each mip level from the level before it, every layer, with their accesses
     * @param firstMipLevel first level to fill, the blits read from firstMipLevel - 1
     * @param mipLevels (VK_REMAINING_MIP_LEVELS => to the last mip level)
     * @param filter depth formats need VK_FILTER_NEAREST
     * @details accesses: the levels from firstMipLevel - 1 to the last filled level in
     *  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, transfer write, then the blits of @ref cvk::Image::blitMipLevels()
     *  with their own barriers
     * @pre the preconditions of @ref cvk::Image::blitMipLevels() without the layouts
     * @post the last state of the levels from firstMipLevel - 1 to the last filled level:
     *  VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, transfer read
     */
    void blitMipLevels(
        cmd_buffer auto& cmd,
        uint32_t firstMipLevel = 1,
        uint32_t mipLevels = VK_REMAINING_MIP_LEVELS,
        VkFilter filter = VK_FILTER_LINEAR
    ) const {
        CTH_CRITICAL(firstMipLevel == 0, "first mip level must be > 0, got {}", firstMipLevel) {}
        auto const levels = mipLevels == VK_REMAINING_MIP_LEVELS ? this->mipLevels() - firstMipLevel : mipLevels;

        auto const write = VK_ACCESS_2_TRANSFER_WRITE_BIT;
        cmd.use(*this, mipAccess(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, write, firstMipLevel - 1, levels + 1));
        // the blits leave each level in VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL after a transfer read
        cvk::Image::blitMipLevels(cmd, firstMipLevel, mipLevels, filter);
        cmd.assume(
            *this,
            ImageState{
                .layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                .scope = {.stages = TRANSFER, .access = VK_ACCESS_2_TRANSFER_READ_BIT},
            },
            firstMipLevel - 1,
            levels + 1
        );
    }

private:
    static constexpr VkPipelineStageFlags2 TRANSFER = VK_PIPELINE_STAGE_2_TRANSFER_BIT;

    /**
     * transfer access of the mip levels
     */
    [[nodiscard]] static constexpr ImageAccess mipAccess(
        VkImageLayout layout,
        VkAccessFlags2 access,
        uint32_t firstMipLevel,
        uint32_t mipLevels
    ) {
        return ImageAccess{
            .layout = layout,
            .stages = TRANSFER,
            .access = access,
            .firstMipLevel = firstMipLevel,
            .mipLevels = mipLevels,
        };
    }
};
}
