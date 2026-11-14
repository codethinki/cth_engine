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
}

void Barrier::add(Image const& image, ImageInfo const& info) {
    Image::debug_check(image);
    CTH_CRITICAL(
        info.firstMipLevel >= image.mipLevels(),
        "first mip level must lie in the image ({}), got {}",
        image.mipLevels(),
        info.firstMipLevel
    ) {}
    CTH_CRITICAL(
        info.mipLevels != VK_REMAINING_MIP_LEVELS && info.mipLevels > image.mipLevels() - info.firstMipLevel,
        "mip levels must lie in the image ({}), got {} from {}",
        image.mipLevels(),
        info.mipLevels,
        info.firstMipLevel
    ) {}
    CTH_CRITICAL(
        info.firstArrayLayer >= image.arrayLayers(),
        "first array layer must lie in the image ({}), got {}",
        image.arrayLayers(),
        info.firstArrayLayer
    ) {}
    CTH_CRITICAL(
        info.arrayLayers != VK_REMAINING_ARRAY_LAYERS && info.arrayLayers > image.arrayLayers() - info.firstArrayLayer,
        "array layers must lie in the image ({}), got {} from {}",
        image.arrayLayers(),
        info.arrayLayers,
        info.firstArrayLayer
    ) {}

    add(image.get(), image.aspectMask(), info);
}

void Barrier::add(BaseBuffer const& buffer, BufferInfo const& info) {
    BaseBuffer::debug_check(buffer);
    CTH_CRITICAL(
        info.offset > buffer.size() || (info.size != VK_WHOLE_SIZE && info.size > buffer.size() - info.offset),
        "range must lie in the buffer ({}), got {} from {}",
        buffer.size(),
        info.size,
        info.offset
    ) {}

    add(buffer.get(), info);
}

void Barrier::debug_check_rendering(
    RenderingInfo const* renderingPtr,
    uint32_t viewMask,
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
        std::popcount(viewMask) > 1 && (flags & VK_DEPENDENCY_VIEW_LOCAL_BIT) == 0,
        "barriers inside a rendering with more than one view need VK_DEPENDENCY_VIEW_LOCAL_BIT"
    ) {}
    auto const transfer = [](VkBufferMemoryBarrier2 const& barrier) {
        return barrier.srcQueueFamilyIndex != barrier.dstQueueFamilyIndex;
    };
    CTH_CRITICAL(
        std::ranges::any_of(bufferBarriers, transfer),
        "barriers inside a rendering have no queue family ownership transfer"
    ) {}

    // a continuing secondary does not know the attachment images
    auto const attached = [renderingPtr](VkImage vkImage) {
        if(renderingPtr == nullptr) return false;
        auto const uses = [vkImage](RenderingAttachment const& attachment) {
            return attachment.view->image().get() == vkImage
                || (attachment.resolveViewPtr != nullptr && attachment.resolveViewPtr->image().get() == vkImage);
        };
        return std::ranges::any_of(renderingPtr->colorAttachments, uses)
            || (renderingPtr->depthAttachment.has_value() && uses(*renderingPtr->depthAttachment))
            || (renderingPtr->stencilAttachment.has_value() && uses(*renderingPtr->stencilAttachment));
    };

    for(auto const& barrier : imageBarriers) {
        CTH_CRITICAL(barrier.oldLayout != barrier.newLayout, "barriers inside a rendering have no layout transition") {}
        CTH_CRITICAL(
            barrier.srcQueueFamilyIndex != barrier.dstQueueFamilyIndex,
            "barriers inside a rendering have no queue family ownership transfer"
        ) {}
        CTH_CRITICAL(
            attached(barrier.image) && barrier.newLayout != VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR
            && barrier.newLayout != VK_IMAGE_LAYOUT_GENERAL,
            "attachments inside a rendering must be in the rendering local read or general layout, got {}",
            barrier.newLayout
        ) {}
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
    CTH_CRITICAL(
        !transition.has_value(),
        "layout transition {} -> {} has no scope for a layout, use add() with scopes",
        oldLayout,
        newLayout
    ) {}

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

    if(cmdBuffer.insideRendering())
        debug_check_rendering(cmdBuffer.rendering(), cmdBuffer.renderingViewMask(), memoryBarriers, buffers, images);

    VkDependencyInfo const dependency{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .pNext = nullptr,
        .dependencyFlags = flags,
        .memoryBarrierCount = static_cast<uint32_t>(memoryBarriers.size()),
        .pMemoryBarriers = memoryBarriers.data(),
        .bufferMemoryBarrierCount = static_cast<uint32_t>(buffers.size()),
        .pBufferMemoryBarriers = buffers.data(),
        .imageMemoryBarrierCount = static_cast<uint32_t>(images.size()),
        .pImageMemoryBarriers = images.data(),
    };
    cmdBuffer.core().functions()->vkCmdPipelineBarrier2(cmdBuffer.get(), &dependency);
}
}
