#include "HlcImage.hpp"

#include <stdexcept>

#include <stb_image.h>

#include "../core/CthDevice.hpp"
#include "../core/CthSwapchain.hpp"
#include "../memory/buffer/CthBuffer.hpp"



namespace cth {

Image::Image(Device& device) : device{device}, image{nullptr} {}

void Image::loadImage(const string& path) {
    uint8_t* img = stbi_load(path.c_str(), &width, &height, &channels, STBI_rgb_alpha);

    //TEMP fix this
   /* const auto buffer = make_unique<Buffer>(device, sizeof(img[0]) * 4, width * height, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    buffer->map();
    buffer->writeToBuffer(img);*/
    stbi_image_free(img);

    createThisImage(VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
    allocateThisImage();
    //stage(buffer->getBuffer());

    imageView = HlcSwapchain::createImageView(device.device(), image, imageInfo.format);

    createDescriptorInfo();
}


void Image::stage(const VkBuffer buffer) {
    transitionImageLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    copyBufferToImage(buffer);
    transitionImageLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}
void Image::createDescriptorInfo() {
    descriptorInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    descriptorInfo.imageView = imageView;
}


void Image::createThisImage(const VkImageTiling tiling, const VkImageUsageFlags usages) {
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    //if(channels == 3) imageInfo.format = VK_FORMAT_R8G8B8_SRGB;
    //else if(channels == 4) imageInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
    //else throw invalid_argument("createImage: unknown channel number");
    imageInfo.format = VK_FORMAT_R8G8B8A8_SRGB;

    imageInfo.tiling = tiling;
    imageInfo.initialLayout = imageLayout;
    imageInfo.usage = usages;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.flags = 0; //
    if(vkCreateImage(device.device(), &imageInfo, nullptr, &image) != VK_SUCCESS) throw runtime_error("createImage: failed to create image");
}
void Image::allocateThisImage() {
    vkGetImageMemoryRequirements(device.device(), image, &memoryRequirements);

    allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateInfo.allocationSize = memoryRequirements.size;
    allocateInfo.memoryTypeIndex = device.findMemoryType(memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if(vkAllocateMemory(device.device(), &allocateInfo, nullptr, &imageMemory) != VK_SUCCESS)
        throw runtime_error(
            "allocateImage: failed to allocate memory");

    vkBindImageMemory(device.device(), image, imageMemory, 0);
}

void Image::transitionImageLayout(const VkImageLayout new_layout) {
    const auto commandBuffer = device.beginSingleTimeCommands();

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = imageLayout;
    barrier.newLayout = new_layout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags sourceFlags;
    VkPipelineStageFlags destinationFlags;

    if(imageLayout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        sourceFlags = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationFlags = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if(imageLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        sourceFlags = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationFlags = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else throw invalid_argument("transitionImageLayout: layout transition undefined");

    vkCmdPipelineBarrier(commandBuffer, sourceFlags, destinationFlags, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    device.endSingleTimeCommands(commandBuffer);

    imageLayout = new_layout;
}
void Image::copyBufferToImage(const VkBuffer buffer) const {
    const auto commandBuffer = device.beginSingleTimeCommands();

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1};
    vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    device.endSingleTimeCommands(commandBuffer);
}

Image::~Image() {
    vkDestroyImageView(device.device(), imageView, nullptr);

    vkDestroyImage(device.device(), image, nullptr);
    vkFreeMemory(device.device(), imageMemory, nullptr);
}

void Image::createImage(const uint32_t width, const uint32_t height, const uint32_t mip_levels, const VkSampleCountFlagBits num_samples,
    const VkFormat format, const VkImageTiling tiling,
    const VkImageUsageFlags usage, const VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& image_memory, const Device& device) {
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = mip_levels;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = num_samples;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if(vkCreateImage(device.device(), &imageInfo, nullptr, &image) != VK_SUCCESS) throw std::runtime_error("failed to create image!");

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device.device(), image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = device.findMemoryType(memRequirements.memoryTypeBits, properties);

    if(vkAllocateMemory(device.device(), &allocInfo, nullptr, &image_memory) != VK_SUCCESS) throw std::runtime_error(
        "failed to allocate image memory!");

    vkBindImageMemory(device.device(), image, image_memory, 0);
}
}
