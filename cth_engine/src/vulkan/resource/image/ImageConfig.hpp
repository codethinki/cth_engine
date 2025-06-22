#pragma once
namespace cth::vk {
struct ImageConfig {
    VkImageAspectFlagBits aspectMask;
    VkFormat format;
    VkImageUsageFlags usage;
    VkMemoryPropertyFlags memoryProperties;
    VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL;
    uint32_t mipLevels = 1;
    VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
    VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    [[nodiscard]] VkImageCreateInfo createInfo() const;
};
}
