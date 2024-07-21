#pragma once
#include <vulkan/vulkan.h>

#include <vector>

//TEMP this is bad solve the issue differently

namespace cth::vk {

struct SubpassDescription {
    SubpassDescription(VkPipelineBindPoint const bind_point,
        std::vector<VkAttachmentReference> const& color_attachments,
        VkAttachmentReference const& depth_attachment,
        std::vector<VkAttachmentReference> const& resolve_attachments) : depthAttachment(depth_attachment),
        colorAttachments(color_attachments), resolveAttachments(resolve_attachments) {

        vkDescription.pipelineBindPoint = bind_point;

        vkDescription.colorAttachmentCount = static_cast<uint32_t>(colorAttachments.size());
        vkDescription.pColorAttachments = colorAttachments.data();
        vkDescription.pDepthStencilAttachment = &depthAttachment;
        vkDescription.pResolveAttachments = resolveAttachments.data();

    }

    [[nodiscard]] VkSubpassDescription get() const { return vkDescription; }
    [[nodiscard]] VkSubpassDescription* ptr() { return &vkDescription; }
    VkAttachmentReference depthAttachment;
    std::vector<VkAttachmentReference> colorAttachments;
    std::vector<VkAttachmentReference> resolveAttachments;

    VkSubpassDescription vkDescription{};
};
}
