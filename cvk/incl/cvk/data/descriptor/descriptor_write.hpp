#pragma once
#include "cvk/data/buffer/base_buffer.hpp"
#include "cvk/data/img/image_view.hpp"
#include "cvk/data/img/sampler.hpp"
#include "cvk/utility/types.hpp"

#include <volk.h>

#include <cstdint>
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
    not_null<BaseBuffer const*> buffer;

    /**
     * @brief in bytes
     */
    size_t offset = 0;

    /**
     * @brief in bytes (VK_WHOLE_SIZE => to the end)
     */
    size_t size = VK_WHOLE_SIZE;

    [[nodiscard]] constexpr VkDescriptorBufferInfo info() const { return buffer->descriptorInfo(size, offset); }
};

/**
 * @brief descriptors for consecutive array elements of one binding
 * @details image bindings take @ref images, buffer bindings take @ref buffers
 */
struct DescriptorWrite {
    uint32_t binding = 0;
    uint32_t arrayElement = 0;

    std::vector<ImageDescriptor> images{};
    std::vector<BufferDescriptor> buffers{};
};
}
