#pragma once
#include "jvk/cmd/cmd_buffer.hpp"

#include "cvk/data/buffer/buffer.hpp"

#include <volk.h>

#include <cth/meta/concepts.hpp>

#include <cstddef>
#include <cstdint>


namespace cvk {
class Image;
}

namespace jvk {
/**
 * cvk buffer whose copies record their accesses in the resource states of the command buffer
 * @tparam T element, the gpu copies it byte by byte
 * @details the copies take a jvk command buffer and record the barriers that their accesses need,
 *  the untracked copies of @ref cvk::Buffer stay reachable through cvk::Buffer<T>::
 */
template<cth::mta::trivially_copyable T>
class Buffer : public cvk::Buffer<T> {
public:
    using cvk::Buffer<T>::Buffer;

    /**
     * records a copy from the source buffer, with its accesses
     * @param region in elements
     * @details accesses: src: transfer stage, transfer read, this buffer: transfer stage, transfer write
     * @pre the preconditions of @ref cvk::Buffer::copy()
     */
    void copy(cmd_buffer auto&, cvk::Buffer<T> const& src, cvk::BufferCopy region = {}) const;

    /**
     * records a copy from the mip level of the image, with its accesses
     * @param dstOffset in elements
     * @param layout of the mip level, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL or VK_IMAGE_LAYOUT_GENERAL
     * @details accesses: the mip level in the layout, transfer stage, transfer read,
     *  this buffer: transfer stage, transfer write
     * @pre the preconditions of @ref cvk::Image::copyTo()
     */
    void copy(
        cmd_buffer auto&,
        cvk::Image const& src,
        size_t dstOffset = 0,
        uint32_t mipLevel = 0,
        VkImageLayout layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
    ) const;
};
}
