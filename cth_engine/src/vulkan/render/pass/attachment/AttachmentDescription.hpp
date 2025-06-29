#pragma once



namespace cth::vk {
class ImageView;

struct AttachmentDescription {
    VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
    VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    VkAttachmentStoreOp storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    VkAttachmentLoadOp stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    VkAttachmentStoreOp stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    VkImageLayout finalLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkImageLayout referenceLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkAttachmentDescriptionFlags flags = 0;

    [[nodiscard]] VkAttachmentDescription create(VkFormat format, VkImageLayout initial_layout) const;
};
}
