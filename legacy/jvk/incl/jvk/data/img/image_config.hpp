#pragma once
namespace jvk {
struct ImageConfig {
    VkImageAspectFlagBits aspectMask;
    VkFormat format;
    VkImageUsageFlags usage;
    std::optional<VkMemoryPropertyFlags> memoryProperties;
    VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL;
    uint32_t mipLevels = 1;
    VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
    VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    [[nodiscard]] static ImageConfig DepthBuffer(VkFormat depth_format, VkSampleCountFlagBits samples) {
        return jvk::ImageConfig{
            .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
            .format = depth_format,
            .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            .memoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            .samples = samples,
        };
    }

    [[nodiscard]] VkImageCreateInfo createInfo() const;
};

}