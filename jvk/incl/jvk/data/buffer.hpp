#pragma once
#include "jvk/cmd/cmd_buffer.hpp"
#include "jvk/sync/resource_states.hpp"

#include "cvk/data/buffer/buffer.hpp"
#include "cvk/data/img/image.hpp"

#include <volk.h>

#include <cth/meta/concepts.hpp>

#include <cstddef>
#include <cstdint>


namespace jvk {
/**
 * tracked buffer
 * @details its copies track their accesses in the jvk command buffer
 */
template<cth::mta::trivially_copyable T>
class Buffer : public cvk::Buffer<T> {
public:
    using cvk::Buffer<T>::Buffer;

    /**
     * records a copy from the source buffer
     * @details tracks the accesses of both buffers
     * @pre the preconditions of @ref cvk::Buffer::copy()
     */
    void copy(cmd_buffer auto& cmd, cvk::Buffer<T> const& src, cvk::BufferCopy region = {}) const {
        cmd.use(src, BufferAccess{.stages = TRANSFER, .access = VK_ACCESS_2_TRANSFER_READ_BIT});
        cmd.use(*this, BufferAccess{.stages = TRANSFER, .access = VK_ACCESS_2_TRANSFER_WRITE_BIT});
        cvk::Buffer<T>::copy(cmd, src, region);
    }

    /**
     * records a copy from the mip level of the image
     * @details tracks the accesses of the image and the buffer
     * @pre the preconditions of @ref cvk::Image::copyTo()
     */
    void copy(
        cmd_buffer auto& cmd,
        cvk::Image const& src,
        size_t dstOffset = 0,
        uint32_t mipLevel = 0,
        VkImageLayout layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
    ) const {
        cmd.use(
            src,
            ImageAccess{
                .layout = layout,
                .stages = TRANSFER,
                .access = VK_ACCESS_2_TRANSFER_READ_BIT,
                .firstMipLevel = mipLevel,
                .mipLevels = 1,
            }
        );
        cmd.use(*this, BufferAccess{.stages = TRANSFER, .access = VK_ACCESS_2_TRANSFER_WRITE_BIT});
        cvk::Buffer<T>::copy(cmd, src, dstOffset, mipLevel, layout);
    }

private:
    static constexpr VkPipelineStageFlags2 TRANSFER = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
};
}
