#pragma once
#include <vulkan/vulkan.h>

#include <vector>


namespace cth {
using namespace std;

struct SubpassDescription {
    SubpassDescription(const VkPipelineBindPoint bind_point,
        const vector<VkAttachmentReference>& color_attachments,
        const VkAttachmentReference& depth_attachment,
        const vector<VkAttachmentReference>& resolve_attachments) : depthAttachment(depth_attachment),
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
    vector<VkAttachmentReference> colorAttachments;
    vector<VkAttachmentReference> resolveAttachments;

    VkSubpassDescription vkDescription{};
};
}
