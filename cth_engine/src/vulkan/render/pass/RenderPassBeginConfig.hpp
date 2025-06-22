#pragma once


namespace cth::vk {

struct RenderPassBeginConfig {
    std::span<VkClearValue const> clearValues;
    VkExtent2D extent;
    VkSubpassContents subpassContents = VK_SUBPASS_CONTENTS_INLINE;
    VkOffset2D offset{0, 0};
};

}