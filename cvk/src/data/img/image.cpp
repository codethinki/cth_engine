#include "cvk/data/img/image.hpp"

#include "cvk/base/allocator.hpp"
#include "cvk/base/core.hpp"
#include "cvk/data/buffer/base_buffer.hpp"
#include "cvk/render/cmd/cmd_buffer.hpp"
#include "cvk/utility/vk_exceptions.hpp"

#include <algorithm>
#include <bit>


namespace cvk {
Image::Image(Core const& core, Config const& config) : _handle{core}, _config{config} {
    Core::debug_check(core);
    Config::debug_check(config);
}

Image::Image(Core const& core, Config const& config, State const& state) : Image{core, config} { wrap(state); }

Image::Image(Core const& core, Config const& config, VkExtent2D extent) : Image{core, config} { create(extent); }

void Image::wrap(State const& state) {
    optDestroy();

    _handle.reset(state.vkImage.get(), core());
    if(state.memory.has_value()) _memory.emplace(core(), _config.memory, *state.memory);
    _extent = state.extent;
}

void Image::create(VkExtent2D extent) {
    optDestroy();

    auto const imageInfo = _config.createInfo(extent);
    auto const allocationInfo = _config.memory.createInfo();

    VkImage vkImage = VK_NULL_HANDLE;
    VmaAllocation vmaAllocation = VK_NULL_HANDLE;
    auto const result = vmaCreateImage(
        core().allocator().get(),
        &imageInfo,
        &allocationInfo,
        &vkImage,
        &vmaAllocation,
        nullptr
    );

    CTH_STABLE_ERR(result != VK_SUCCESS, "failed to create image") {
        forget();
        throw cvk::vk_result_exception{result, details->exception()};
    }

    _handle.reset(vkImage, core());
    _memory.emplace(core(), _config.memory, Memory::State{vmaAllocation});
    _extent = extent;
}

void Image::destroy() {
    debug_check(*this);

    _handle.reset();
    _memory.reset();

    forget();
}

Image::State Image::release() {
    debug_check(*this);

    State state{
        .vkImage = _handle.release(),
        .extent = _extent,
        .memory = std::nullopt,
    };
    if(_memory.has_value()) state.memory = _memory->release();

    forget();
    return state;
}

void Image::copy(
    BaseCmdBuffer const& cmdBuffer,
    BaseBuffer const& src,
    size_t srcOffset,
    uint32_t mipLevel,
    VkImageLayout layout
) const {
    auto const region = bufferCopy(cmdBuffer, src, srcOffset, mipLevel);
    CTH_CRITICAL(
        (src.config().usage & VK_BUFFER_USAGE_TRANSFER_SRC_BIT) == 0,
        "src usage must have VK_BUFFER_USAGE_TRANSFER_SRC_BIT"
    ) {}
    CTH_CRITICAL(
        (_config.usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT) == 0,
        "image usage must have VK_IMAGE_USAGE_TRANSFER_DST_BIT"
    ) {}

    CTH_CRITICAL(
        layout != VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && layout != VK_IMAGE_LAYOUT_GENERAL,
        "mip level must be in VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL or VK_IMAGE_LAYOUT_GENERAL"
    ) {
        details->add("layout: {}", layout);
    }

    VkCopyBufferToImageInfo2 const info{
        .sType = VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2,
        .pNext = nullptr,
        .srcBuffer = src.get(),
        .dstImage = get(),
        .dstImageLayout = layout,
        .regionCount = 1,
        .pRegions = &region,
    };
    core().functions()->vkCmdCopyBufferToImage2(cmdBuffer.get(), &info);
}

void Image::copyTo(
    BaseCmdBuffer const& cmdBuffer,
    BaseBuffer const& dst,
    size_t dstOffset,
    uint32_t mipLevel,
    VkImageLayout layout
) const {
    auto const region = bufferCopy(cmdBuffer, dst, dstOffset, mipLevel);
    CTH_CRITICAL(
        (dst.config().usage & VK_BUFFER_USAGE_TRANSFER_DST_BIT) == 0,
        "dst usage must have VK_BUFFER_USAGE_TRANSFER_DST_BIT"
    ) {}
    CTH_CRITICAL(
        (_config.usage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) == 0,
        "image usage must have VK_IMAGE_USAGE_TRANSFER_SRC_BIT"
    ) {}

    CTH_CRITICAL(
        layout != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL && layout != VK_IMAGE_LAYOUT_GENERAL,
        "mip level must be in VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL or VK_IMAGE_LAYOUT_GENERAL"
    ) {
        details->add("layout: {}", layout);
    }

    VkCopyImageToBufferInfo2 const info{
        .sType = VK_STRUCTURE_TYPE_COPY_IMAGE_TO_BUFFER_INFO_2,
        .pNext = nullptr,
        .srcImage = get(),
        .srcImageLayout = layout,
        .dstBuffer = dst.get(),
        .regionCount = 1,
        .pRegions = &region,
    };
    core().functions()->vkCmdCopyImageToBuffer2(cmdBuffer.get(), &info);
}

VkBufferImageCopy2 Image::bufferCopy(
    BaseCmdBuffer const& cmdBuffer,
    BaseBuffer const& buffer,
    size_t bufferOffset,
    uint32_t mipLevel
) const {
    debug_check(*this);
    BaseBuffer::debug_check(buffer);
    CTH_CRITICAL(!cmdBuffer.recording(), "command buffer must be recording") {}
    CTH_CRITICAL(mipLevel >= mipLevels(), "mip level must lie in the image") {}
    CTH_CRITICAL(!std::has_single_bit(aspectMask()), "a copy needs exactly one aspect") {}

    return VkBufferImageCopy2{
        .sType = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2,
        .pNext = nullptr,
        .bufferOffset = bufferOffset,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource = {
            .aspectMask = aspectMask(),
            .mipLevel = mipLevel,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
        .imageOffset = {0, 0, 0},
        .imageExtent = {std::max(_extent.width >> mipLevel, 1u), std::max(_extent.height >> mipLevel, 1u), 1},
    };
}

void Image::destroy(DeviceTable table, VkImage vkImage) {
    CTH_WARN(vkImage == VK_NULL_HANDLE, "vkImage invalid (VK_NULL_HANDLE)") {}

    table->vkDestroyImage(table.device(), vkImage, nullptr);
}

void Image::forget() {
    _memory.reset();
    _extent = {};
}
}
