#pragma once


namespace cth::vk {
//TEMP left off here. the begin config should not be a part of the render pass config. the begin config with extent and offset can change dynamically depending e.g. on the framebuffer and swapchain
struct RenderPassBeginConfig {
    VkSubpassContents subpassContents = VK_SUBPASS_CONTENTS_INLINE;

    /**
     * @note optional, can be specified after create
     */
    std::vector<VkClearValue> clearValues{};
    /**
     * @brief optional, can be specified after create
     */
    VkExtent2D extent{0, 0};
    /**
     * @brief optional, can be specified after create
     */
    VkOffset2D offset{0, 0};
};

}
