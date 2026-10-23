#include "jvk/surface/swapchain/swapchain_subpass_config.hpp"

#include <ranges>

namespace jvk {
SwapchainSubpassConfig SwapchainSubpassConfig::ColorAttachmentOptimal(
    std::span<uint32_t const> subpass_indices
) {
    return {
        .subpassLayouts = {
            std::from_range,
            subpass_indices | std::views::transform(
                [](auto index) { return std::pair{index, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}; }
            )
        },
        .imageUsageFlags = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageAttachmentLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
    };
}

SwapchainSubpassConfig SwapchainSubpassConfig::ColorAttachmentOptimal(uint32_t subpass_index) {
    return ColorAttachmentOptimal(std::array<uint32_t, 1>{{subpass_index}});
}

}
