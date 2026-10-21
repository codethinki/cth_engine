#include "cvk/render/sync/barrier.hpp"

#include "cvk/base/core.hpp"
#include "cvk/data/buffer/base_buffer.hpp"
#include "cvk/data/img/image.hpp"
#include "cvk/data/img/image_view.hpp"
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

    VkMemoryBarrier2 memory_barrier(MemoryBarrierInfo const& info) {
        bool const transfer = info.src.queueFamilyIndex != VK_QUEUE_FAMILY_IGNORED
            || info.dst.queueFamilyIndex != VK_QUEUE_FAMILY_IGNORED;
        CTH_CRITICAL(transfer, "memory barriers have no queue family ownership transfer") {}

        return VkMemoryBarrier2{
            .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
            .pNext = nullptr,
            .srcStageMask = info.src.stages,
            .srcAccessMask = info.src.access,
            .dstStageMask = info.dst.stages,
            .dstAccessMask = info.dst.access,
        };
    }

    VkBufferMemoryBarrier2 buffer_barrier(BaseBuffer const& buffer, BufferBarrierInfo const& info) {
        BaseBuffer::debug_check(buffer);
        CTH_CRITICAL(
            info.offset > buffer.size() || (info.size != VK_WHOLE_SIZE && info.size > buffer.size() - info.offset),
            "range must lie in the buffer"
        ) {}

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

    VkImageMemoryBarrier2 image_barrier(Image const& image, ImageBarrierInfo const& info) {
        Image::debug_check(image);
        CTH_CRITICAL(info.mipLevels == 0, "mip levels must not be 0") {}
        CTH_CRITICAL(info.firstMipLevel >= image.mipLevels(), "first mip level must lie in the image") {}
        CTH_CRITICAL(
            info.mipLevels != VK_REMAINING_MIP_LEVELS && info.mipLevels > image.mipLevels() - info.firstMipLevel,
            "mip levels must lie in the image"
        ) {}
        CTH_CRITICAL(info.arrayLayers == 0, "array layers must not be 0") {}
        CTH_CRITICAL(info.firstArrayLayer >= image.arrayLayers(), "first array layer must lie in the image") {}
        CTH_CRITICAL(
            info.arrayLayers != VK_REMAINING_ARRAY_LAYERS
            && info.arrayLayers > image.arrayLayers() - info.firstArrayLayer,
            "array layers must lie in the image"
        ) {}

        bool const transfer = info.src.queueFamilyIndex != info.dst.queueFamilyIndex;
        CTH_CRITICAL(
            (info.oldLayout != info.newLayout || transfer)
            && (info.newLayout == VK_IMAGE_LAYOUT_UNDEFINED || info.newLayout == VK_IMAGE_LAYOUT_PREINITIALIZED),
            "a transition or queue family transfer must not have an undefined or preinitialized new layout"
        ) {
            details->add("transition: {} -> {}", info.oldLayout, info.newLayout);
        }

        return VkImageMemoryBarrier2{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .pNext = nullptr,
            .srcStageMask = info.src.stages,
            .srcAccessMask = info.src.access,
            .dstStageMask = info.dst.stages,
            .dstAccessMask = info.dst.access,
            .oldLayout = info.oldLayout,
            .newLayout = info.newLayout,
            .srcQueueFamilyIndex = info.src.queueFamilyIndex,
            .dstQueueFamilyIndex = info.dst.queueFamilyIndex,
            .image = image.get(),
            .subresourceRange = {
                .aspectMask = info.aspectMask != 0 ? info.aspectMask : image.aspectMask(),
                .baseMipLevel = info.firstMipLevel,
                .levelCount = info.mipLevels,
                .baseArrayLayer = info.firstArrayLayer,
                .layerCount = info.arrayLayers,
            },
        };
    }
}

void Barrier::debug_check_rendering(
    RenderingInfo const& rendering,
    std::span<VkMemoryBarrier2 const> memoryBarriers,
    std::span<VkBufferMemoryBarrier2 const> bufferBarriers,
    std::span<VkImageMemoryBarrier2 const> imageBarriers
) const {
    bool srcStages = false;
    bool framebufferSpace = true;
    auto const check = [&](VkPipelineStageFlags2 src, VkPipelineStageFlags2 dst) {
        srcStages |= src != VK_PIPELINE_STAGE_2_NONE;
        framebufferSpace &= framebuffer_space(src, dst);
    };

    for(auto const& barrier : memoryBarriers)
        check(barrier.srcStageMask, barrier.dstStageMask);
    for(auto const& barrier : bufferBarriers)
        check(barrier.srcStageMask, barrier.dstStageMask);
    for(auto const& barrier : imageBarriers)
        check(barrier.srcStageMask, barrier.dstStageMask);

    CTH_CRITICAL(!framebufferSpace, "barriers inside a rendering must only use framebuffer space stages") {}
    CTH_CRITICAL(
        srcStages && (flags & VK_DEPENDENCY_BY_REGION_BIT) == 0,
        "barriers inside a rendering need VK_DEPENDENCY_BY_REGION_BIT"
    ) {}
    CTH_CRITICAL(
        std::popcount(rendering.viewMask) > 1 && (flags & VK_DEPENDENCY_VIEW_LOCAL_BIT) == 0,
        "barriers inside a rendering with more than one view need VK_DEPENDENCY_VIEW_LOCAL_BIT"
    ) {}
    auto const transfer = [](VkBufferMemoryBarrier2 const& barrier) {
        return barrier.srcQueueFamilyIndex != barrier.dstQueueFamilyIndex;
    };
    CTH_CRITICAL(
        std::ranges::any_of(bufferBarriers, transfer),
        "barriers inside a rendering have no queue family ownership transfer"
    ) {}

    auto const attached = [&rendering](Image const& image) {
        auto const uses = [&image](RenderingAttachment const& attachment) {
            return &attachment.view->image() == &image
                || (attachment.resolveView != nullptr && &attachment.resolveView->image() == &image);
        };
        return std::ranges::any_of(rendering.colorAttachments, uses)
            || (rendering.depthAttachment.has_value() && uses(*rendering.depthAttachment))
            || (rendering.stencilAttachment.has_value() && uses(*rendering.stencilAttachment));
    };

    for(auto const [barrier, entry] : std::views::zip(imageBarriers, images)) {
        CTH_CRITICAL(barrier.oldLayout != barrier.newLayout, "barriers inside a rendering have no layout transition") {}
        CTH_CRITICAL(
            barrier.srcQueueFamilyIndex != barrier.dstQueueFamilyIndex,
            "barriers inside a rendering have no queue family ownership transfer"
        ) {}
        CTH_CRITICAL(
            attached(*entry.image) && barrier.newLayout != VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR
            && barrier.newLayout != VK_IMAGE_LAYOUT_GENERAL,
            "attachments inside a rendering must be in the rendering local read or general layout"
        ) {
            details->add("layout: {}", barrier.newLayout);
        }
    }
}

void Barrier::addTransition(
    Image const& image,
    VkImageLayout newLayout,
    VkImageLayout oldLayout,
    VkPipelineStageFlags2 shaderStages,
    uint32_t firstMipLevel,
    uint32_t mipLevels
) {
    auto const transition = layout_transition(newLayout, oldLayout, image.aspectMask(), shaderStages);
    CTH_CRITICAL(!transition.has_value(), "layout transition has no scope for a layout, use add() with scopes") {
        details->add("transition: {} -> {}", oldLayout, newLayout);
    }

    add(
        image,
        ImageInfo::LayoutTransition(newLayout, oldLayout, transition->dst, transition->src, firstMipLevel, mipLevels)
    );
}

void Barrier::execute(BaseCmdBuffer const& cmdBuffer) const {
    BaseCmdBuffer::debug_check(cmdBuffer);
    CTH_CRITICAL(!cmdBuffer.recording(), "command buffer must be recording") {}

    std::vector<VkMemoryBarrier2> memoryBarriers{};
    memoryBarriers.reserve(memory.size());
    for(auto const& info : memory)
        memoryBarriers.push_back(memory_barrier(info));

    std::vector<VkBufferMemoryBarrier2> bufferBarriers{};
    bufferBarriers.reserve(buffers.size());
    for(auto const& [buffer, info] : buffers)
        bufferBarriers.push_back(buffer_barrier(*buffer, info));

    std::vector<VkImageMemoryBarrier2> imageBarriers{};
    imageBarriers.reserve(images.size());
    for(auto const& [image, info] : images)
        imageBarriers.push_back(image_barrier(*image, info));

    if(auto const* rendering = cmdBuffer.rendering())
        debug_check_rendering(*rendering, memoryBarriers, bufferBarriers, imageBarriers);

    VkDependencyInfo const dependency{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .pNext = nullptr,
        .dependencyFlags = flags,
        .memoryBarrierCount = static_cast<uint32_t>(memoryBarriers.size()),
        .pMemoryBarriers = memoryBarriers.data(),
        .bufferMemoryBarrierCount = static_cast<uint32_t>(bufferBarriers.size()),
        .pBufferMemoryBarriers = bufferBarriers.data(),
        .imageMemoryBarrierCount = static_cast<uint32_t>(imageBarriers.size()),
        .pImageMemoryBarriers = imageBarriers.data(),
    };
    cmdBuffer.core().functions()->vkCmdPipelineBarrier2(cmdBuffer.get(), &dependency);
}
}
