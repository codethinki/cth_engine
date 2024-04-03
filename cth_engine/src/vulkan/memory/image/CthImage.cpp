#include "CthImage.hpp"


#include "vulkan/base/CthDevice.hpp"
#include "vulkan/memory/buffer/CthDefaultBuffer.hpp"
#include "vulkan/utility/CthVkUtils.hpp"

#include <cth/cth_log.hpp>


namespace cth {
using namespace std;


Image::Image(Device* device, const VkExtent2D extent, const Config& config) : device(device), _extent(extent), _config{config} {
    CTH_ERR(!(config.usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT), "image must have transfer destination usage")
        throw details->exception();


    create();
    allocate();
    bind();
}
Image::~Image() {
    vkDestroyImage(device->get(), vkImage, nullptr);
    vkFreeMemory(device->get(), vkImageMemory, nullptr);
}
void Image::create() {
    auto createInfo = _config.createInfo();

    createInfo.extent.width = _extent.width;
    createInfo.extent.height = _extent.height;

    const auto createResult = vkCreateImage(device->get(), &createInfo, nullptr, &vkImage);

    CTH_STABLE_ERR(createResult != VK_SUCCESS, "failed to create image")
        throw except::vk_result_exception{createResult, details->exception()};
}

void Image::createView() {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = vkImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = _config.format;
    viewInfo.subresourceRange.aspectMask = _config.aspectFlags;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = _config.mipLevels;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;
    VkImageView imageView;
    if(vkCreateImageView(device->get(), &viewInfo, nullptr, &imageView) != VK_SUCCESS)
        throw runtime_error(
            "createImageView: failed to create image view");
}


void Image::allocate() {
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device->get(), vkImage, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = device->findMemoryType(memRequirements.memoryTypeBits, _config.memoryProperties);

    const auto allocResult = vkAllocateMemory(device->get(), &allocInfo, nullptr, &vkImageMemory);

    CTH_STABLE_ERR(allocResult != VK_SUCCESS, "failed to allocate image memory")
        throw except::vk_result_exception{allocResult, details->exception()};
}
void Image::bind() const {
    const auto bindResult = vkBindImageMemory(device->get(), vkImage, vkImageMemory, 0);

    CTH_STABLE_ERR(bindResult != VK_SUCCESS, "failed to bind image memory")
        throw except::vk_result_exception{bindResult, details->exception()};
}
void Image::write(const DefaultBuffer* buffer) const {
    const auto commandBuffer = device->beginSingleTimeCommands();

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {_extent.width, _extent.height, 1};
    vkCmdCopyBufferToImage(commandBuffer, buffer->get(), vkImage, imageLayout, 1, &region);

    device->endSingleTimeCommands(commandBuffer);
}


Image::transition_config Image::transitionConfig(const VkImageLayout current_layout, const VkImageLayout new_layout) {
    if(current_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
        return {0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT};
    if(current_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        return {VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT};

    CTH_ERR(true, "unsupported layout transition") {
        details->add("transition: {0} -> {1}", static_cast<uint32_t>(current_layout), static_cast<uint32_t>(new_layout));
        throw details->exception();
    }

    return {};
}

void Image::transitionLayout(const VkImageLayout new_layout) {
    VkCommandBuffer commandBuffer = device->beginSingleTimeCommands();

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = imageLayout;
    barrier.newLayout = new_layout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = vkImage;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; //TEMP make this configurable
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = _config.mipLevels;
    //currently only 2d images
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    auto [srcAccess, dstAccess, srcStage, dstStage] = transitionConfig(imageLayout, new_layout);
    barrier.srcAccessMask = srcAccess;
    barrier.dstAccessMask = dstAccess;

    vkCmdPipelineBarrier(commandBuffer, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    device->endSingleTimeCommands(commandBuffer);

    imageLayout = new_layout;
}

} // namespace cth

//Config

namespace cth {
VkImageCreateInfo Image::Config::createInfo() const {
    VkImageCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;

    info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.imageType = VK_IMAGE_TYPE_2D;
    info.arrayLayers = 1;
    info.extent.depth = 1;

    info.format = format;
    info.tiling = tiling;
    info.usage = usage;
    info.mipLevels = mipLevels;
    info.samples = samples;

    return info;
}
} // namespace cth
