#include "CthTexture.hpp"

#include "vulkan/render/cmd/CthCmdBuffer.hpp"
#include "vulkan/render/control/CthPipelineBarrier.hpp"
#include "vulkan/resource/buffer/CthBuffer.hpp"

namespace cth::vk {


Texture::Texture(not_null<BasicCore const*> core, VkExtent2D extent, Config const& config, CmdBuffer const& cmd_buffer,
    std::span<char const> staging_data) : Image{core, imageConfig(extent, config), extent} {

    Buffer<char> buffer{core, staging_data.size(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};
    buffer.map();
    buffer.write(staging_data);

    init(cmd_buffer, buffer);
}
Texture::Texture(not_null<BasicCore const*> core, VkExtent2D extent, Config const& config, CmdBuffer const& cmd_buffer,
    BasicBuffer const& staging_buffer, size_t buffer_offset) : Image{core, imageConfig(extent, config),
    extent} { init(cmd_buffer, staging_buffer, buffer_offset); }


void Texture::init(CmdBuffer const& cmd_buffer, BasicBuffer const& buffer, size_t offset) {
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

void Texture::blitMipLevels(CmdBuffer const& cmd_buffer, int32_t first, int32_t levels) {
    if(levels == 0) levels = mipLevels() - first;

    CTH_ERR(_levelLayouts[first - 1] != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, "src layout not transfer src optimal")
        throw details->exception();
    CTH_ERR(std::ranges::any_of(_levelLayouts.begin() + first, _levelLayouts.begin() + first + levels, [](const VkImageLayout layout){\
        return layout != VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL; }), "image layouts not transfer dst optimal")
        throw details->exception();

    ImageBarrier toSrcBarrier{VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT};
    ImageBarrier shaderBarrier{VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT};

    auto width = static_cast<int32_t>(extent().width);
    auto height = static_cast<int32_t>(extent().height);

    for(int32_t i = first; i < first + levels; i++) {
        if(i != first) {
            transitionLayout(toSrcBarrier, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, i - 1, 1);

            toSrcBarrier.execute(cmd_buffer);
            toSrcBarrier.remove(this);
        }

        VkImageBlit blit{};
        blit.srcOffsets[0] = {0, 0, 0};
        blit.srcOffsets[1] = {(width >> (i - 1)), (height >> (i - 1)), 1};
        blit.srcSubresource.aspectMask = aspectMask();
        blit.srcSubresource.mipLevel = i - 1;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount = 1;
        blit.dstOffsets[0] = {0, 0, 0};
        blit.dstOffsets[1] = {width > 1 ? width / 2 : 1, height > 1 ? height / 2 : 1, 1};
        blit.dstSubresource.aspectMask = aspectMask();
        blit.dstSubresource.mipLevel = i;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount = 1;

        vkCmdBlitImage(cmd_buffer.get(), get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit,
            VK_FILTER_LINEAR);

        if(width > 1) width >>= 1;
        if(height > 1) height >>= 1;

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