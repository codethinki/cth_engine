#include "CthTexture.hpp"

#include "vulkan/memory/buffer/CthBuffer.hpp"


namespace cth {

Texture::Texture(Device* device, const VkExtent2D extent, const Config& config, const span<const char> staging_data) : Image(device, extent, imageConfig(extent, config)) {
    Buffer<char> buffer{device, staging_data.size(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};
    buffer.map();
    buffer.write(staging_data);

    init(&buffer);
}
Texture::Texture(Device* device, const VkExtent2D extent, const Config& config, const DefaultBuffer* buffer, const size_t buffer_offset) : Image(device, extent, imageConfig(extent, config)) {
    init(buffer, buffer_offset);
}

void Texture::init(const DefaultBuffer* buffer, const size_t offset) {
    Image::transitionLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    Image::write(buffer, offset);

    genMipmaps(1);
}

Image::Config Texture::imageConfig(const VkExtent2D extent, const Config& config) {
    return Image::Config{
        config.aspectMask,
        config.format,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        VK_IMAGE_TILING_OPTIMAL,
        Image::evalMipLevelCount(extent),
        VK_SAMPLE_COUNT_1_BIT
    };
}

void Texture::genMipmaps(const int32_t first, int32_t levels) {
    if(levels == 0) levels = mipLevels() - first;

    CTH_ERR(any_of(imageLayouts.begin() + first, imageLayouts.begin() + first + levels, [](const VkImageLayout layout){\
        return layout != VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL; }), "image layouts not transfer dst optimal")
        throw details->exception();


    VkCommandBuffer commandBuffer = device->beginSingleTimeCommands();

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.image = get();
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask = aspectMask();
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    const auto width = static_cast<int32_t>(extent().width);
    const auto height = static_cast<int32_t>(extent().height);

    for(int32_t i = first; i < first + levels; i++) {
        barrier.subresourceRange.baseMipLevel = i - 1;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
            0, nullptr, 0, nullptr, 1, &barrier);

        VkImageBlit blit{};
        blit.srcOffsets[0] = {0, 0, 0};
        blit.srcOffsets[1] = {(width >> (i - 1)), (height >> (i - 1)), 1};
        blit.srcSubresource.aspectMask = aspectMask();
        blit.srcSubresource.mipLevel = i - 1;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount = 1;
        blit.dstOffsets[0] = {0, 0, 0};
        blit.dstOffsets[1] = {(width >> i), (height >> i), 1};
        blit.dstSubresource.aspectMask = aspectMask();
        blit.dstSubresource.mipLevel = i;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount = 1;

        vkCmdBlitImage(commandBuffer, get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    }
    barrier.subresourceRange.baseMipLevel = first + levels - 1;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, 
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    device->endSingleTimeCommands(commandBuffer);


}

}
