#pragma once


#include <volk.h>

#include <span>
#include <unordered_map>

namespace jvk {
struct SwapchainSubpassConfig {
    std::unordered_map<uint32_t, VkImageLayout> subpassLayouts;


    /**
     * @brief swapchain image usage flags
     */
    VkImageUsageFlags imageUsageFlags = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT |
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    /**
     * @brief swapchain image attachment load op
     */
    VkAttachmentLoadOp imageAttachmentLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;

    static SwapchainSubpassConfig ColorAttachmentOptimal(std::span<uint32_t const> subpass_indices);
    static SwapchainSubpassConfig ColorAttachmentOptimal(uint32_t subpass_index);
};
}