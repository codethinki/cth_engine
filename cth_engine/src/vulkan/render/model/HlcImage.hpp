#pragma once
#include <string>
#include <vulkan/vulkan.h>

//TODO add inheritance from descripted resource class

namespace cth {
using namespace std;
class Device;

class oldImage {
public:
    explicit oldImage(Device& device);
    ~oldImage();

    void loadImage(const string& path);

    [[nodiscard]] VkDescriptorImageInfo getDescriptorInfo(const VkSampler& sampler) {
        descriptorInfo.sampler = sampler;
        return descriptorInfo;
    }

    oldImage(const oldImage&) = delete;
    oldImage& operator=(const oldImage&) = delete;

    int width, height, channels;


    static void createImage(uint32_t width, uint32_t height, uint32_t mip_levels, VkSampleCountFlagBits num_samples, VkFormat format,
        VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& image_memory,
        const Device& device);

private:
    Device& device;
    VkImage image;
    VkImageLayout imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkImageView imageView;
    VkDeviceMemory imageMemory;
    VkImageCreateInfo imageInfo{};
    VkDescriptorImageInfo descriptorInfo{};
    VkMemoryRequirements memoryRequirements;
    VkMemoryAllocateInfo allocateInfo{};
    void createThisImage(VkImageTiling tiling, VkImageUsageFlags usages);
    void allocateThisImage();
    void transitionImageLayout(VkImageLayout new_layout);
    void copyBufferToImage(VkBuffer buffer) const;
    void stage(VkBuffer buffer);
    void createDescriptorInfo();
};


}
