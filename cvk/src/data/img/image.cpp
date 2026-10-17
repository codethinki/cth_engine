#include "cvk/data/img/image.hpp"

#include "cvk/base/allocator.hpp"
#include "cvk/base/core.hpp"
#include "cvk/data/buffer/base_buffer.hpp"
#include "cvk/render/cmd/cmd_buffer.hpp"
#include "cvk/render/sync/barrier.hpp"
#include "cvk/utility/vk_exceptions.hpp"

#include <algorithm>
#include <bit>


namespace cvk {
Image::Image(Core const& core, Config const& config) : _handle{core}, _memory{core, config.memory},
    _config{config}, _levelLayouts(config.mipLevels, config.initialLayout) {
    Core::debug_check(core);
    Config::debug_check(config);
}

Image::Image(Core const& core, Config const& config, State const& state) : Image{core, config} { wrap(state); }

Image::Image(Core const& core, Config const& config, VkExtent2D extent) : Image{core, config} { create(extent); }

void Image::wrap(State const& state) {
    CTH_CRITICAL(state.levelLayouts.size() > _config.mipLevels, "more level layouts than mip levels") {}

    optDestroy();

    _handle.reset(state.vkImage.get(), core());
    if(state.memory.has_value()) _memory.wrap(*state.memory);
    _extent = state.extent;
    _levelLayouts.assign(_config.mipLevels, _config.initialLayout);
    std::ranges::copy(state.levelLayouts, _levelLayouts.begin());
}

void Image::create(VkExtent2D extent) {
    optDestroy();

    auto const imageInfo = _config.createInfo(extent);
    auto const allocationInfo = _memory.config().createInfo();

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
    _memory.wrap(Memory::State{vmaAllocation});
    _extent = extent;
    _levelLayouts.assign(_config.mipLevels, _config.initialLayout);
}

void Image::destroy() {
    debug_check(*this);

    _handle.reset();
    _memory.optDestroy();

    forget();
}

Image::State Image::release() {
    debug_check(*this);

    State state{
        .vkImage = _handle.release(),
        .extent = _extent,
        .memory = std::nullopt,
        .levelLayouts = std::move(_levelLayouts),
    };
    if(_memory.created()) state.memory = _memory.release();

    forget();
    return state;
}

void Image::copy(BaseCmdBuffer const& cmdBuffer, BaseBuffer const& src, size_t srcOffset, uint32_t mipLevel) const {
    debug_check(*this);
    BaseBuffer::debug_check(src);
    CTH_CRITICAL(!cmdBuffer.recording(), "command buffer must be recording") {}
    CTH_CRITICAL(mipLevel >= mipLevels(), "mip level must lie in the image") {}
    CTH_CRITICAL(!std::has_single_bit(aspectMask()), "a copy needs exactly one aspect") {}
    CTH_CRITICAL(
        (src.config().usage & VK_BUFFER_USAGE_TRANSFER_SRC_BIT) == 0,
        "src usage must have VK_BUFFER_USAGE_TRANSFER_SRC_BIT"
    ) {}
    CTH_CRITICAL(
        (_config.usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT) == 0,
        "image usage must have VK_IMAGE_USAGE_TRANSFER_DST_BIT"
    ) {}

    auto const layout = _levelLayouts[mipLevel];
    CTH_CRITICAL(
        layout != VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && layout != VK_IMAGE_LAYOUT_GENERAL,
        "mip level must be in VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL or VK_IMAGE_LAYOUT_GENERAL"
    ) {
        details->add("layout: {}", layout);
    }

    VkBufferImageCopy2 const region{
        .sType = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2,
        .pNext = nullptr,
        .bufferOffset = srcOffset,
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

void Image::transitionLayout(
    BaseCmdBuffer const& cmdBuffer,
    VkImageLayout newLayout,
    uint32_t firstMipLevel,
    uint32_t mipLevels
) {
    debug_check(*this);

    auto const oldLayout = layout(firstMipLevel);
    auto const transition = layout_transition(oldLayout, newLayout);
    CTH_CRITICAL(!transition.has_value(), "layout transition has no table entry, use a Barrier") {
        details->add("transition: {} -> {}", oldLayout, newLayout);
    }

    Barrier barrier{};
    barrier.add(
        *this,
        ImageBarrierInfo::LayoutTransition(newLayout, transition->src, transition->dst, firstMipLevel, mipLevels)
    );
    barrier.execute(cmdBuffer);
}

void Image::setLayout(uint32_t firstMipLevel, uint32_t mipLevels, VkImageLayout layout) {
    CTH_CRITICAL(firstMipLevel >= _levelLayouts.size(), "first mip level must lie in the image") {}

    size_t const count = mipLevels == VK_REMAINING_MIP_LEVELS ? _levelLayouts.size() - firstMipLevel : mipLevels;
    CTH_CRITICAL(count > _levelLayouts.size() - firstMipLevel, "mip levels must lie in the image") {}

    std::ranges::fill_n(_levelLayouts.begin() + firstMipLevel, static_cast<ptrdiff_t>(count), layout);
}

void Image::destroy(DeviceTable table, VkImage vkImage) {
    CTH_WARN(vkImage == VK_NULL_HANDLE, "vkImage invalid (VK_NULL_HANDLE)") {}

    table->vkDestroyImage(table.device(), vkImage, nullptr);
}

void Image::forget() {
    _extent = {};
    _levelLayouts.assign(_config.mipLevels, _config.initialLayout);
}
}
