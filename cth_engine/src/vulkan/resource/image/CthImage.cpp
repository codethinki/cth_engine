#include "CthImage.hpp"


#include "vulkan/base/CthDevice.hpp"
#include "vulkan/pipeline/CthPipelineBarrier.hpp"
#include "vulkan/resource/buffer/CthDefaultBuffer.hpp"
#include "vulkan/utility/CthVkUtils.hpp"

#include <cth/cth_log.hpp>

#include <stb_image.h>

namespace cth {
using namespace std;


Image::Image(Device* device, const VkExtent2D extent, const BasicImage::Config& config) : BasicImage(extent, config, VK_NULL_HANDLE), device(device) {

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
void Image::write(const DefaultBuffer* buffer, const size_t offset, const uint32_t mip_level) const {
    CTH_WARN(imageLayouts[mip_level] != VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,\
        "PERFORMANCE: image layout is not transfer dst optional");


    const auto commandBuffer = device->beginSingleTimeCommands();

    VkBufferImageCopy region;
    region.bufferOffset = offset;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;

    //const auto mask = aspect_mask == VK_IMAGE_ASPECT_NONE ? _config.aspectFlags : aspect_mask;
    region.imageSubresource.aspectMask = _config.aspectMask;

    region.imageSubresource.mipLevel = mip_level;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {_extent.width, _extent.height, 1};
    vkCmdCopyBufferToImage(commandBuffer, buffer->get(), vkImage, imageLayouts[mip_level], 1, &region);

    device->endSingleTimeCommands(commandBuffer);
}


Image::transition_config Image::transitionConfig(const VkImageLayout current_layout, const VkImageLayout new_layout) {
    if(current_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
        return {0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT};
    if(current_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        return {VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT};
    if(current_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
        return {VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT};
    if(current_layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        return {VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT};
    CTH_ERR(true, "unsupported layout transition") {
        details->add("transition: {0} -> {1}", static_cast<uint32_t>(current_layout), static_cast<uint32_t>(new_layout));
        throw details->exception();
    }

    return {};
}

void Image::transitionLayout(const VkImageLayout new_layout, const uint32_t first_mip_level, const uint32_t mip_levels) {
    const auto levels = mip_levels == 0 ? _config.mipLevels - first_mip_level : mip_levels;

    const auto oldLayout = imageLayouts[first_mip_level];
    CTH_ERR(any_of(imageLayouts.begin() + first_mip_level, imageLayouts.begin() + first_mip_level + levels,\
        [oldLayout](VkImageLayout layout){ return oldLayout != layout; }), "all transitioned layouts must be the same")
        throw details->exception();

    auto [srcAccess, dstAccess, srcStage, dstStage] = transitionConfig(oldLayout, new_layout);

    ImageBarrier barrier{srcStage, dstStage, {{this, ImageBarrier::Info::LayoutTransition(srcAccess, dstAccess, new_layout, first_mip_level, mip_levels)}}};


    VkCommandBuffer commandBuffer = device->beginSingleTimeCommands();

    barrier.execute(commandBuffer);

    device->endSingleTimeCommands(commandBuffer);
}
uint32_t Image::evalMipLevelCount(const VkExtent2D extent) {
    return static_cast<uint32_t>(std::floor(std::log2(std::max(extent.width, extent.height))) + 1);
}

} // namespace cth
