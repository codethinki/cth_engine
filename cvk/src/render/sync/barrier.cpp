#include "cvk/render/sync/barrier.hpp"

#include "cvk/base/core.hpp"
#include "cvk/data/buffer/base_buffer.hpp"
#include "cvk/data/img/image.hpp"
#include "cvk/render/cmd/cmd_buffer.hpp"

#include <cth/io/log.hpp>

#include <algorithm>
#include <bit>
#include <span>


namespace cvk {
namespace {
    constexpr VkPipelineStageFlags2 FRAMEBUFFER_STAGES = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT
        | VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT
        | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT
        | VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

    [[nodiscard]] constexpr bool framebuffer_space(VkPipelineStageFlags2 srcStages, VkPipelineStageFlags2 dstStages) {
        return ((srcStages | dstStages) & ~FRAMEBUFFER_STAGES) == 0;
    }

    VkBufferMemoryBarrier2 buffer_barrier(BaseBuffer const& buffer, BufferBarrierInfo const& info) {
        BaseBuffer::debug_check(buffer);

        return VkBufferMemoryBarrier2{
            .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
            .pNext = nullptr,
            .srcStageMask = info.src.stages,
            .srcAccessMask = info.src.access,
            .dstStageMask = info.dst.stages,
            .dstAccessMask = info.dst.access,
            .srcQueueFamilyIndex = info.src.queueFamilyIndex,
            .dstQueueFamilyIndex = info.dst.queueFamilyIndex,
            .buffer = buffer.get(),
            .offset = info.offset,
            .size = info.size,
        };
    }

    /**
     * @brief image barrier from the current layout of the mip range
     */
    VkImageMemoryBarrier2 image_barrier(Image const& image, ImageBarrierInfo const& info) {
        Image::debug_check(image);

        size_t const count = info.mipLevels == VK_REMAINING_MIP_LEVELS ? std::dynamic_extent : info.mipLevels;
        auto const layouts = image.layouts().subspan(info.firstMipLevel, count);
        auto const oldLayout = layouts.front();

        CTH_CRITICAL(
            std::ranges::any_of(layouts, [oldLayout](VkImageLayout layout) { return layout != oldLayout; }),
            "every mip level of the range must have one layout"
        ) {}

        auto const newLayout = info.newLayout.value_or(oldLayout);
        CTH_CRITICAL(
            newLayout == VK_IMAGE_LAYOUT_UNDEFINED || newLayout == VK_IMAGE_LAYOUT_PREINITIALIZED,
            "new layout must not be undefined or preinitialized"
        ) {
            details->add("image layout: {}", oldLayout);
        }

        return VkImageMemoryBarrier2{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .pNext = nullptr,
            .srcStageMask = info.src.stages,
            .srcAccessMask = info.src.access,
            .dstStageMask = info.dst.stages,
            .dstAccessMask = info.dst.access,
            .oldLayout = oldLayout,
            .newLayout = newLayout,
            .srcQueueFamilyIndex = info.src.queueFamilyIndex,
            .dstQueueFamilyIndex = info.dst.queueFamilyIndex,
            .image = image.get(),
            .subresourceRange = {
                .aspectMask = info.aspectMask != 0 ? info.aspectMask : image.aspectMask(),
                .baseMipLevel = info.firstMipLevel,
                .levelCount = info.mipLevels,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
        };
    }
}

void Barrier::add(Image& image, ImageInfo const& info) {
    Image::debug_check(image);
    CTH_CRITICAL(info.mipLevels == 0, "mip levels must not be 0") {}
    CTH_CRITICAL(info.firstMipLevel >= image.mipLevels(), "first mip level must lie in the image") {}
    CTH_CRITICAL(
        info.mipLevels != VK_REMAINING_MIP_LEVELS && info.mipLevels > image.mipLevels() - info.firstMipLevel,
        "mip levels must lie in the image"
    ) {}

    _images.emplace_back(&image, info);
}

void Barrier::add(BaseBuffer const& buffer, BufferInfo const& info) {
    BaseBuffer::debug_check(buffer);
    CTH_CRITICAL(
        info.offset > buffer.size() || (info.size != VK_WHOLE_SIZE && info.size > buffer.size() - info.offset),
        "range must lie in the buffer"
    ) {}

    _buffers.emplace_back(&buffer, info);
}

void Barrier::debug_check_rendering(
    BaseCmdBuffer::Rendering const& rendering,
    std::span<VkBufferMemoryBarrier2 const> bufferBarriers,
    std::span<VkImageMemoryBarrier2 const> imageBarriers
) const {
    bool srcStages = false;
    bool framebufferSpace = true;
    auto const check = [&](VkPipelineStageFlags2 src, VkPipelineStageFlags2 dst) {
        srcStages |= src != VK_PIPELINE_STAGE_2_NONE;
        framebufferSpace &= framebuffer_space(src, dst);
    };

    for(auto const& barrier : _memoryBarriers)
        check(barrier.srcStageMask, barrier.dstStageMask);
    for(auto const& barrier : bufferBarriers)
        check(barrier.srcStageMask, barrier.dstStageMask);
    for(auto const& barrier : imageBarriers)
        check(barrier.srcStageMask, barrier.dstStageMask);

    CTH_CRITICAL(!framebufferSpace, "barriers inside a rendering must only use framebuffer space stages") {}
    CTH_CRITICAL(
        srcStages && (_flags & VK_DEPENDENCY_BY_REGION_BIT) == 0,
        "barriers inside a rendering need VK_DEPENDENCY_BY_REGION_BIT"
    ) {}
    CTH_CRITICAL(
        std::popcount(rendering.inheritance.viewMask) > 1 && (_flags & VK_DEPENDENCY_VIEW_LOCAL_BIT) == 0,
        "barriers inside a rendering with more than one view need VK_DEPENDENCY_VIEW_LOCAL_BIT"
    ) {}
    auto const transfer = [](VkBufferMemoryBarrier2 const& barrier) {
        return barrier.srcQueueFamilyIndex != barrier.dstQueueFamilyIndex;
    };
    CTH_CRITICAL(
        std::ranges::any_of(bufferBarriers, transfer),
        "barriers inside a rendering have no queue family ownership transfer"
    ) {}

    for(auto const [barrier, entry] : std::views::zip(imageBarriers, _images)) {
        CTH_CRITICAL(barrier.oldLayout != barrier.newLayout, "barriers inside a rendering have no layout transition") {}
        CTH_CRITICAL(
            barrier.srcQueueFamilyIndex != barrier.dstQueueFamilyIndex,
            "barriers inside a rendering have no queue family ownership transfer"
        ) {}
        bool const attachment = std::ranges::contains(
            rendering.attachments,
            static_cast<Image const*>(entry.image.get()),
            &not_null<Image const*>::get
        );
        CTH_CRITICAL(
            attachment && barrier.newLayout != VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR
            && barrier.newLayout != VK_IMAGE_LAYOUT_GENERAL,
            "attachments inside a rendering must be in the rendering local read or general layout"
        ) {
            details->add("layout: {}", barrier.newLayout);
        }
    }
}

void Barrier::execute(BaseCmdBuffer const& cmdBuffer) {
    BaseCmdBuffer::debug_check(cmdBuffer);
    CTH_CRITICAL(!cmdBuffer.recording(), "command buffer must be recording") {}

    std::vector<VkBufferMemoryBarrier2> bufferBarriers{};
    bufferBarriers.reserve(_buffers.size());
    for(auto const& [buffer, info] : _buffers)
        bufferBarriers.push_back(buffer_barrier(*buffer, info));

    std::vector<VkImageMemoryBarrier2> imageBarriers{};
    imageBarriers.reserve(_images.size());
    for(auto const& [image, info] : _images)
        imageBarriers.push_back(image_barrier(*image, info));

    if(auto const* rendering = cmdBuffer.rendering()) debug_check_rendering(*rendering, bufferBarriers, imageBarriers);

    VkDependencyInfo const dependency{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .pNext = nullptr,
        .dependencyFlags = _flags,
        .memoryBarrierCount = static_cast<uint32_t>(_memoryBarriers.size()),
        .pMemoryBarriers = _memoryBarriers.data(),
        .bufferMemoryBarrierCount = static_cast<uint32_t>(bufferBarriers.size()),
        .pBufferMemoryBarriers = bufferBarriers.data(),
        .imageMemoryBarrierCount = static_cast<uint32_t>(imageBarriers.size()),
        .pImageMemoryBarriers = imageBarriers.data(),
    };
    cmdBuffer.core().functions()->vkCmdPipelineBarrier2(cmdBuffer.get(), &dependency);

    for(auto const& [image, info] : _images)
        if(info.newLayout.has_value()) image->setLayout(info.firstMipLevel, info.mipLevels, *info.newLayout);
}
}
