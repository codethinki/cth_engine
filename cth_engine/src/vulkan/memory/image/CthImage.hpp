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
    virtual ~Image();


    void write(const DefaultBuffer* buffer, size_t offset = 0, uint32_t mip_level = 0) const;

    /**
     * \brief transitions the image layout via a pipeline barrier
     * \param mip_levels (0 => all remaining)
     */
    void transitionLayout(VkImageLayout new_layout, uint32_t first_mip_level = 0, uint32_t mip_levels = 0);

    [[nodiscard]] static uint32_t evalMipLevelCount(VkExtent2D extent);
private:
    void create();
    void allocate();
    void bind() const;

    using transition_config = struct {
        VkAccessFlags srcAccess, dstAccess;
        VkPipelineStageFlags srcStage, vkPipelineStage;
    };
    static transition_config transitionConfig(VkImageLayout current_layout, VkImageLayout new_layout);



    VkImage vkImage = VK_NULL_HANDLE;
    VkDeviceMemory vkImageMemory = VK_NULL_HANDLE;
    VkExtent2D _extent;

public:
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
    Device* device;
    vector<VkImageLayout> imageLayouts{};


private:
    Config _config;
    friend ImageView;

public:
    [[nodiscard]] VkImage get() const { return vkImage; }
    [[nodiscard]] VkFormat format() const { return _config.format; }
    [[nodiscard]] VkExtent2D extent() const { return _extent; }
    [[nodiscard]] uint32_t mipLevels() const { return _config.mipLevels; }
    [[nodiscard]] VkImageLayout layout(const uint32_t mip_level = 0) const { return imageLayouts[mip_level]; }
    [[nodiscard]] vector<VkImageLayout> layouts() const { return imageLayouts; }
    [[nodiscard]] VkImageAspectFlagBits aspectMask() const { return _config.aspectMask; }
};

} // namespace cth

//TODO implement multidimensional image support
//void write(const DefaultBuffer* buffer, size_t offset = 0, uint32_t mip_level = 0, VkImageAspectFlagBits aspect_mask = VK_IMAGE_ASPECT_NONE) const;
//transitionLayout(VkImageLayout new_layout, uint32_t first_mip_level = 0, uint32_t mip_levels = VK_WHOLE_SIZE, VkImageAspectFlagBits aspect_mask = VK_IMAGE_ASPECT_NONE);
