#pragma once
#include "cvk/data/img/image.hpp"
#include "cvk/data/img/image_view_config.hpp"

#include <volk.h>

#include <cth/ptr/ref_wrapper.hpp>


namespace cvk {
/**
 * sampled image descriptor
 * @details `OpTypeImage` with `Sampled` 1 in the shader: `texture2D`
 * @details the image usage has `VK_IMAGE_USAGE_SAMPLED_BIT`
 */
struct SampledImageDescriptor {
    cth::ref_wrapper<Image const> image;
    ImageViewConfig view{};
    /** layout of the image at the shader access */
    VkImageLayout layout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
};

/**
 * storage image descriptor
 * @details `OpTypeImage` with `Sampled` 2 in the shader: `image2D`
 * @details the image usage has `VK_IMAGE_USAGE_STORAGE_BIT`
 */
struct StorageImageDescriptor {
    cth::ref_wrapper<Image const> image;
    ImageViewConfig view{};
    VkImageLayout layout = VK_IMAGE_LAYOUT_GENERAL; ///< layout of the image at the shader access
};

/**
 * input attachment descriptor
 * @details `OpTypeImage` with `Dim` `SubpassData` in the shader: `subpassInput`
 * @details the image usage has `VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT`
 */
struct InputAttachmentDescriptor {
    cth::ref_wrapper<Image const> image;
    ImageViewConfig view{};
    /** layout of the image at the shader access */
    VkImageLayout layout = VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR;
};
}
