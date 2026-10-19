#pragma once
#include "cvk/data/buffer/base_buffer.hpp"
#include "cvk/data/img/image_view.hpp"
#include "cvk/data/img/sampler.hpp"
#include "cvk/utility/types.hpp"

#include <volk.h>

#include <cth/io/log.hpp>

#include <cstdint>
#include <variant>
#include <vector>


namespace cvk {
/**
 * @brief image, sampler or both for an image descriptor, the binding decides the descriptor type
 */
struct ImageDescriptor {
    /**
     * @brief nullptr -> no view, VK_DESCRIPTOR_TYPE_SAMPLER
     */
    ImageView const* view = nullptr;

    /**
     * @brief layout of the view when the gpu reads it
     * @details VK_DESCRIPTOR_TYPE_STORAGE_IMAGE -> VK_IMAGE_LAYOUT_GENERAL
     */
    VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    /**
     * @brief nullptr -> no sampler, the descriptor type needs none
     */
    Sampler const* sampler = nullptr;

    [[nodiscard]] constexpr VkDescriptorImageInfo info() const {
        return VkDescriptorImageInfo{
            .sampler = sampler != nullptr ? sampler->get() : VK_NULL_HANDLE,
            .imageView = view != nullptr ? view->get() : VK_NULL_HANDLE,
            .imageLayout = view != nullptr ? layout : VK_IMAGE_LAYOUT_UNDEFINED,
        };
    }
};

/**
 * @brief buffer range for a buffer descriptor, the binding decides the descriptor type
 */
struct BufferDescriptor {
    not_null_ptr<BaseBuffer const> buffer;

    /**
     * @brief in bytes
     */
    size_t offset = 0;

    /**
     * @brief in bytes (VK_WHOLE_SIZE => to the end)
     */
    size_t size = VK_WHOLE_SIZE;

    /**
     * @pre @ref BaseBuffer::created()
     * @pre the range lies in the buffer
     */
    [[nodiscard]] constexpr VkDescriptorBufferInfo info() const {
        BaseBuffer::debug_check(*buffer);
        CTH_CRITICAL(offset > buffer->size(), "offset must lie in the buffer") {}
        CTH_CRITICAL(size != VK_WHOLE_SIZE && size > buffer->size() - offset, "range must lie in the buffer") {}

        return VkDescriptorBufferInfo{
            .buffer = buffer->get(),
            .offset = offset,
            .range = size == VK_WHOLE_SIZE ? buffer->size() - offset : size,
        };
    }
};

/**
 * @brief image descriptors for consecutive array elements of one image binding
 */
struct ImageDescriptorWrite {
    uint32_t binding = 0;
    uint32_t arrayElement = 0;
    std::vector<ImageDescriptor> images{};
};

/**
 * @brief buffer descriptors for consecutive array elements of one buffer binding
 */
struct BufferDescriptorWrite {
    uint32_t binding = 0;
    uint32_t arrayElement = 0;
    std::vector<BufferDescriptor> buffers{};
};

/**
 * @brief descriptors for consecutive array elements of one binding, the kind matches the binding type
 */
using DescriptorWrite = std::variant<ImageDescriptorWrite, BufferDescriptorWrite>;
}
