#pragma once
//TEMP rename this to CthBasicImage.hpp
#include <vector>
#include <vulkan/vulkan.h>

namespace cth {
class ImageBarrier;
}

namespace cth {
using namespace std;

class ImageView;

/**
 * \brief wrapper for VkImage with no ownership
 */
class BasicImage {
public:
    struct Config;

    BasicImage(VkExtent2D extent, const Config& config, VkImage image);
    virtual ~BasicImage() = default;


    struct Config {
        VkImageAspectFlagBits aspectMask;
        VkFormat format;
        VkImageUsageFlags usage;
        VkMemoryPropertyFlags memoryProperties;
        VkImageTiling tiling;
        uint32_t mipLevels;
        VkSampleCountFlagBits samples;

        [[nodiscard]] VkImageCreateInfo createInfo() const;
    };

protected:
    VkExtent2D _extent;
    Config _config;
    VkImage vkImage = VK_NULL_HANDLE;
    vector<VkImageLayout> imageLayouts{};

    friend ImageBarrier;

public:
    [[nodiscard]] VkImage get() const { return vkImage; }
    [[nodiscard]] VkFormat format() const { return _config.format; }
    [[nodiscard]] VkExtent2D extent() const { return _extent; }
    [[nodiscard]] uint32_t mipLevels() const { return _config.mipLevels; }
    [[nodiscard]] VkImageLayout layout(const uint32_t mip_level = 0) const { return imageLayouts[mip_level]; }
    [[nodiscard]] vector<VkImageLayout> layouts() const { return imageLayouts; }
    [[nodiscard]] VkImageAspectFlagBits aspectMask() const { return _config.aspectMask; }
    [[nodiscard]] BasicImage::Config config() const { return _config; }
};
} // namespace cth
