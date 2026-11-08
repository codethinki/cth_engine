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
 * tracked image
 * @details its commands track their accesses in the jvk command buffer
 * @details a created image is in its creation layout
 */
class Image : public cvk::Image {
public:
    using cvk::Image::Image;

    /**
     * records a copy from the buffer into every layer of the mip level
     * @details tracks the accesses of the image and the buffer with one barrier
     * @pre the preconditions of `cvk::Image::copy()`
     */
    void copy(
        cmd_buffer auto& cmd,
        cvk::BaseBuffer const& src,
        size_t srcOffset = 0,
        uint32_t mipLevel = 0,
        VkImageLayout layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
    ) const {
        cmd.use(
            *this,
            mipAccess(layout, VK_ACCESS_2_TRANSFER_WRITE_BIT, mipLevel, 1),
            src,
            BufferAccess{.stages = TRANSFER, .access = VK_ACCESS_2_TRANSFER_READ_BIT}
        );
        cvk::Image::copy(cmd, src, srcOffset, mipLevel, layout);
    }

    /**
     * records a copy from every layer of the mip level into the buffer
     * @details tracks the accesses of the image and the buffer with one barrier
     * @pre the preconditions of `cvk::Image::copyTo()`
     */
    void copyTo(
        cmd_buffer auto& cmd,
        cvk::BaseBuffer const& dst,
        size_t dstOffset = 0,
        uint32_t mipLevel = 0,
        VkImageLayout layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
    ) const {
        cmd.use(
            *this,
            mipAccess(layout, VK_ACCESS_2_TRANSFER_READ_BIT, mipLevel, 1),
            dst,
            BufferAccess{.stages = TRANSFER, .access = VK_ACCESS_2_TRANSFER_WRITE_BIT}
        );
        cvk::Image::copyTo(cmd, dst, dstOffset, mipLevel, layout);
    }

    /**
     * records blits that fill each mip level from the level before it
     * @details tracks the accesses of the mip levels
     * @pre the preconditions of `cvk::Image::blitMipLevels()` without the layouts
     * @post the levels from `firstMipLevel - 1`: `VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL`, transfer read
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
        // cvk leaves the levels in VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
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
