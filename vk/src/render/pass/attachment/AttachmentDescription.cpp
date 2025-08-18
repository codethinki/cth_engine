#include "vk/render/pass/attachment/AttachmentDescription.hpp"

namespace cth::vk {

auto AttachmentDescription::create(VkFormat format, VkImageLayout initial_layout) const -> VkAttachmentDescription {
    return VkAttachmentDescription{
        .flags = flags,
        .format = format,
        .samples = samples,
        .loadOp = loadOp,
        .storeOp = storeOp,
        .stencilLoadOp = stencilLoadOp,
        .stencilStoreOp = stencilStoreOp,
        .initialLayout = initial_layout,
        .finalLayout = finalLayout
    };
}
}
