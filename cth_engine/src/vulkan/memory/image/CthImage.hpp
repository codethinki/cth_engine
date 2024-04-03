#pragma once
#include <vulkan/vulkan.h>

#include <string_view>
#include <vector>

#include "../descriptor/CthDescriptor.hpp"

namespace cth {
using namespace std;

class Device;
class DefaultBuffer;
class Sampler;
class ImageView;
class Image;


class Image {
    struct TransitionConfig;

public:
    struct Config;


    explicit Image(Device* device, VkExtent2D extent, const Config& config);
    ~Image();

    [[nodiscard]] vector<uint8_t> loadImage(string_view path);

    void write(const DefaultBuffer* buffer) const;
    void transitionLayout(VkImageLayout new_layout);

private:
    void create();
    void createView();
    void allocate();
    void bind() const;

    using transition_config = struct {
        VkAccessFlags srcAccess, dstAccess;
        VkPipelineStageFlags srcStage, vkPipelineStage;
    };
    static transition_config transitionConfig(VkImageLayout current_layout, VkImageLayout new_layout);


    Device* device;
    VkExtent2D _extent;

    VkImageLayout imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkImage vkImage = VK_NULL_HANDLE;
    VkDeviceMemory vkImageMemory = VK_NULL_HANDLE;

public:
    struct Config {
        VkImageAspectFlags aspectFlags;
        VkFormat format;
        VkImageUsageFlags usage;
        VkMemoryPropertyFlags memoryProperties;
        VkImageTiling tiling;
        uint32_t mipLevels;
        VkSampleCountFlagBits samples;

        [[nodiscard]] VkImageCreateInfo createInfo() const;
    };

private:
    Config _config;
    friend ImageView;
public:
    [[nodiscard]] VkImage get() const { return vkImage; }
    [[nodiscard]] VkFormat format() const { return _config.format; }
    [[nodiscard]] VkExtent2D extent() const { return _extent; }
    [[nodiscard]] uint32_t mipLevels() const { return _config.mipLevels; }
    [[nodiscard]] VkImageLayout layout() const { return imageLayout; }
    [[nodiscard]] VkImageAspectFlags aspectFlags() const { return _config.aspectFlags; }
};

} // namespace cth
