#include "CthTexture.hpp"

#include "vulkan/render/cmd/CthCmdBuffer.hpp"
#include "vulkan/render/control/CthPipelineBarrier.hpp"
#include "vulkan/resource/buffer/CthBuffer.hpp"

namespace cth::vk {


Texture::Texture(cth::not_null<Core const*> core, VkExtent2D extent, Config const& config, CmdBuffer const& cmd_buffer,
    std::span<char const> staging_data) : Image{core, imageConfig(extent, config), extent} {

    Buffer<char> buffer{core, staging_data.size(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};
    buffer.map();
    buffer.write(staging_data);

    init(cmd_buffer, buffer);
}
Texture::Texture(cth::not_null<Core const*> core, VkExtent2D extent, Config const& config, CmdBuffer const& cmd_buffer,
    BaseBuffer const& staging_buffer, size_t buffer_offset) : Image{core, imageConfig(extent, config),
    extent} { init(cmd_buffer, staging_buffer, buffer_offset); }

void Texture::init(CmdBuffer const& cmd_buffer, BaseBuffer const& buffer, size_t offset) {
    CTH_ERR(_levelLayouts[0] == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, "already initialized")
        throw details->exception();

    Image::transitionLayout(cmd_buffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    Image::copy(cmd_buffer, buffer, offset);
    Image::transitionLayout(cmd_buffer, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 0, 1);

    blitMipLevels(cmd_buffer);

    Image::transitionLayout(cmd_buffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 1);
}

Image::Config Texture::imageConfig(VkExtent2D extent, Config const& config) {
    return Image::Config{
        config.aspectMask,
        config.format,
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        VK_IMAGE_TILING_OPTIMAL,
        Image::evalMipLevelCount(extent),
        VK_SAMPLE_COUNT_1_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED
    };
}

void Texture::blitMipLevels(CmdBuffer const& cmd_buffer, uint32_t first, uint32_t levels) {
    if(levels == 0) levels = mipLevels() - first;

    CTH_ERR(_levelLayouts[first - 1] != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, "src layout not transfer src optimal")
        throw details->exception();
    CTH_ERR(std::ranges::any_of(_levelLayouts.begin() + first, _levelLayouts.begin() + first + levels, [](VkImageLayout const layout){\
        return layout != VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL; }), "image layouts not transfer dst optimal")
        throw details->exception();

    ImageBarrier toSrcBarrier{VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT};
    ImageBarrier shaderBarrier{VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT};

    auto const extent = this->extent();
    auto width = static_cast<int32_t>(extent.width);
    auto height = static_cast<int32_t>(extent.height);

    for(uint32_t i = first; i < first + levels; i++) {
        if(i != first) {
            transitionLayout(toSrcBarrier, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, i - 1, 1);

            toSrcBarrier.execute(cmd_buffer);
            toSrcBarrier.remove(this);
        }

        auto const hWidth = width > 1 ? width / 2 : 1;
        auto const hHeight = height > 1 ? height / 2 : 1;

        VkImageBlit blit{
            .srcSubresource{
                .aspectMask = static_cast<VkImageAspectFlags>(aspectMask()),
                .mipLevel = i - 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
            .srcOffsets = {{0, 0, 0}, {width, height, 1}},
            .dstSubresource{
                .aspectMask = static_cast<VkImageAspectFlags>(aspectMask()),
                .mipLevel = i,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
            .dstOffsets = {{0, 0, 0}, {hWidth, hHeight, 1}},
        };
        vkCmdBlitImage(cmd_buffer.get(), get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit,
            VK_FILTER_LINEAR);

        width = hWidth;
        height = hHeight;

        if(i == first) continue;

        transitionLayout(shaderBarrier, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, i - 1, 1);
        shaderBarrier.execute(cmd_buffer);
        shaderBarrier.remove(this);
    }
    transitionLayout(shaderBarrier, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
        first + levels - 1, 1);
    shaderBarrier.execute(cmd_buffer);
    shaderBarrier.remove(this);
}

}
