#pragma once
#include <map>
#include <volk.h>


namespace jvk {
class ImageView;

struct AttachmentDescription {
    VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
    VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    VkAttachmentStoreOp storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    VkAttachmentLoadOp stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    VkAttachmentStoreOp stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    VkImageLayout finalLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    std::map<uint32_t, VkImageLayout> subpassLayouts{};
    VkAttachmentDescriptionFlags flags = 0;

    [[nodiscard]] static AttachmentDescription DepthBuffer(uint32_t subpass_index,
        VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT) {
        return {
            .samples = samples,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            .subpassLayouts = {{subpass_index, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL}},
        };
    }

    [[nodiscard]] static AttachmentDescription Color(uint32_t subpass_index,
        VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT) {
        return {
            .samples = samples,
            .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .subpassLayouts = {{subpass_index, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}}
        };
    }

    [[nodiscard]] VkAttachmentDescription create(VkFormat format, VkImageLayout initial_layout) const;
};
}