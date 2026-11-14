#pragma once
#include "cvk/base/queue/queue_family.hpp"
#include "cvk/render/cmd/cmd_buffer.hpp"
#include "cvk/utility/format.hpp"
#include "cvk/utility/types.hpp"

#include <volk.h>

#include <cth/io/log.hpp>

#include <optional>
#include <span>
#include <vector>


namespace cvk {
class BaseBuffer;
class Image;

/**
 * one side of a barrier
 */
struct BarrierScope {
    VkPipelineStageFlags2 stages = VK_PIPELINE_STAGE_2_NONE;
    VkAccessFlags2 access = VK_ACCESS_2_NONE;

    /**
     * `VK_QUEUE_FAMILY_IGNORED` -> no queue family ownership transfer
     */
    queue_family_index_t queueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
};

/**
 * image part of a barrier
 */
struct ImageBarrierInfo {
    BarrierScope src{};
    BarrierScope dst{};

    VkImageLayout newLayout = VK_IMAGE_LAYOUT_UNDEFINED; ///< `== oldLayout` -> no layout transition

    /**
     * layout of the mip range before the barrier
     * @attention `VK_IMAGE_LAYOUT_UNDEFINED` discards the contents, the validation layers do not report it
     */
    VkImageLayout oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    uint32_t firstMipLevel = 0;

    uint32_t mipLevels = VK_REMAINING_MIP_LEVELS; ///< `VK_REMAINING_MIP_LEVELS` -> to the last mip level

    uint32_t firstArrayLayer = 0;

    uint32_t arrayLayers = VK_REMAINING_ARRAY_LAYERS; ///< `VK_REMAINING_ARRAY_LAYERS` -> to the last layer

    VkImageAspectFlags aspectMask = 0; ///< `0` -> the aspect mask of the image

    [[nodiscard]] static constexpr ImageBarrierInfo LayoutTransition(
        VkImageLayout newLayout,
        VkImageLayout oldLayout,
        BarrierScope const& dst,
        BarrierScope const& src,
        uint32_t firstMipLevel = 0,
        uint32_t mipLevels = VK_REMAINING_MIP_LEVELS
    ) {
        return ImageBarrierInfo{
            .src = src,
            .dst = dst,
            .newLayout = newLayout,
            .oldLayout = oldLayout,
            .firstMipLevel = firstMipLevel,
            .mipLevels = mipLevels,
        };
    }

    /**
     * queue family ownership transfer without layout transition, `dst` and `src` name the families
     * @param layout of the image
     */
    [[nodiscard]] static constexpr ImageBarrierInfo QueueTransition(
        VkImageLayout layout,
        BarrierScope const& dst,
        BarrierScope const& src
    ) {
        return ImageBarrierInfo{.src = src, .dst = dst, .newLayout = layout, .oldLayout = layout};
    }
};

/**
 * buffer part of a barrier
 */
struct BufferBarrierInfo {
    BarrierScope src{};
    BarrierScope dst{};

    size_t offset = 0; ///< in bytes

    size_t size = VK_WHOLE_SIZE; ///< in bytes (`VK_WHOLE_SIZE` => to the end)

    /**
     * queue family ownership transfer, `dst` and `src` name the families
     */
    [[nodiscard]] static constexpr BufferBarrierInfo QueueTransition(BarrierScope const& dst, BarrierScope const& src) {
        return BufferBarrierInfo{.src = src, .dst = dst};
    }
};

/**
 * global memory part of a barrier
 */
struct MemoryBarrierInfo {
    BarrierScope src{};
    BarrierScope dst{};
};

//TODO review the memory barriers stay infos: they refer to no object, the family check stays in execute()
/**
 * pipeline barrier
 * @details
 * - memory, buffer and image barriers in one `vkCmdPipelineBarrier2()`
 * - the buffer and image barriers are the vulkan structs: they keep no reference to the objects
 */
struct Barrier {
    using MemoryInfo = MemoryBarrierInfo;
    using ImageInfo = ImageBarrierInfo;
    using BufferInfo = BufferBarrierInfo;

    /**
     * shader stages of `addTransition()`: the layout decides, fragment shader for shader reads
     */
    static constexpr VkPipelineStageFlags2 INFERRED_SHADER_STAGES = VK_PIPELINE_STAGE_2_NONE;

    VkDependencyFlags flags = 0; ///< `VK_DEPENDENCY_BY_REGION_BIT` -> the dependency is per framebuffer region

    std::vector<MemoryInfo> memory{};
    std::vector<VkBufferMemoryBarrier2> buffers{};
    std::vector<VkImageMemoryBarrier2> images{};

    /**
     * adds a global memory barrier
     */
    constexpr void add(BarrierScope const& dst, BarrierScope const& src) {
        memory.push_back({.src = src, .dst = dst});
    }

    /**
     * adds an image barrier
     * @pre the image is created
     * @pre the mip and layer ranges lie in the image
     * @pre the preconditions of @ref add(VkImage, VkImageAspectFlags, ImageInfo const&)
     */
    void add(Image const&, ImageInfo const&);

    //TODO review the handle overloads: the add() of the objects calls them, the jvk tracking has no objects
    /**
     * adds an image barrier of the image handle
     * @details
     * - no check of the image: the caller knows its mip levels, layers and aspects
     * - `ImageInfo::aspectMask` is `0` -> the aspect mask of the image
     * @pre mip levels and layers `> 0`
     * @pre layout transition or queue family ownership transfer -> new layout not undefined or preinitialized
     */
    constexpr void add(VkImage, VkImageAspectFlags aspectMask, ImageInfo const&);

    /**
     * adds a layout transition of every layer with the scopes of `layout_transition()`
     * @param shaderStages stages of the shader reads in the layouts (`INFERRED_SHADER_STAGES` => fragment shader)
     * @param mipLevels (`VK_REMAINING_MIP_LEVELS` => to the last mip level)
     * @pre `layout_scope()` knows both layouts
     */
    void addTransition(
        Image const&,
        VkImageLayout newLayout,
        VkImageLayout oldLayout,
        VkPipelineStageFlags2 shaderStages = INFERRED_SHADER_STAGES,
        uint32_t firstMipLevel = 0,
        uint32_t mipLevels = VK_REMAINING_MIP_LEVELS
    );

    /**
     * adds a buffer barrier
     * @pre the buffer is created
     * @pre the range lies in the buffer
     */
    void add(BaseBuffer const&, BufferInfo const&);

    /**
     * adds a buffer barrier of the buffer handle
     * @details no check of the buffer: the caller knows its size
     */
    constexpr void add(VkBuffer, BufferInfo const&);

    /**
     * records the barriers
     * @pre `BaseCmdBuffer::recording()`
     * @pre the images and buffers are not destroyed
     * @pre memory barriers have no queue family ownership transfer
     * @pre `BaseCmdBuffer::insideRendering()` -> framebuffer space stages and `VK_DEPENDENCY_BY_REGION_BIT`
     * @pre `BaseCmdBuffer::insideRendering()` -> no layout transition and no queue family ownership transfer
     * @pre `BaseCmdBuffer::insideRendering()` with multiview -> `VK_DEPENDENCY_VIEW_LOCAL_BIT`
     * @pre `BaseCmdBuffer::rendering()` -> the attachments in a local read or general layout
     */
    void execute(BaseCmdBuffer const&) const;

    /**
     * removes every barrier, keeps the flags
     */
    constexpr void clear() {
        memory.clear();
        buffers.clear();
        images.clear();
    }

    [[nodiscard]] constexpr bool empty() const { return memory.empty() && buffers.empty() && images.empty(); }

private:
    /**
     * checks the rules of barriers inside a dynamic rendering
     * @param renderingPtr the rendering the command buffer began, `nullptr` -> a secondary continues the rendering
     * @param viewMask of the rendering
     */
    void debug_check_rendering(
        RenderingInfo const* renderingPtr,
        uint32_t viewMask,
        std::span<VkMemoryBarrier2 const>,
        std::span<VkBufferMemoryBarrier2 const>,
        std::span<VkImageMemoryBarrier2 const>
    ) const;
};
}

namespace cvk {
constexpr void Barrier::add(VkImage vkImage, VkImageAspectFlags aspectMask, ImageInfo const& info) {
    CTH_CRITICAL(info.mipLevels == 0, "mip levels must not be 0") {}
    CTH_CRITICAL(info.arrayLayers == 0, "array layers must not be 0") {}

    bool const transfer = info.src.queueFamilyIndex != info.dst.queueFamilyIndex;
    CTH_CRITICAL(
        (info.oldLayout != info.newLayout || transfer)
        && (info.newLayout == VK_IMAGE_LAYOUT_UNDEFINED || info.newLayout == VK_IMAGE_LAYOUT_PREINITIALIZED),
        "a transition or queue family transfer must not have an undefined or preinitialized new layout, got {} -> {}",
        info.oldLayout,
        info.newLayout
    ) {}

    images.push_back(VkImageMemoryBarrier2{
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
        .image = vkImage,
        .subresourceRange = {
            .aspectMask = info.aspectMask != 0 ? info.aspectMask : aspectMask,
            .baseMipLevel = info.firstMipLevel,
            .levelCount = info.mipLevels,
            .baseArrayLayer = info.firstArrayLayer,
            .layerCount = info.arrayLayers,
        },
    });
}

constexpr void Barrier::add(VkBuffer vkBuffer, BufferInfo const& info) {
    buffers.push_back(VkBufferMemoryBarrier2{
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
        .pNext = nullptr,
        .srcStageMask = info.src.stages,
        .srcAccessMask = info.src.access,
        .dstStageMask = info.dst.stages,
        .dstAccessMask = info.dst.access,
        .srcQueueFamilyIndex = info.src.queueFamilyIndex,
        .dstQueueFamilyIndex = info.dst.queueFamilyIndex,
        .buffer = vkBuffer,
        .offset = info.offset,
        .size = info.size,
    });
}
}

namespace cvk {
/**
 * scopes of a layout transition
 */
struct LayoutTransition {
    BarrierScope src;
    BarrierScope dst;
};

/**
 * what the gpu does with an image in the layout
 * @details input attachments: fragment shader only
 * @details general: all commands
 * @param aspects of the image, pick color or depth for the attachment, read only and local read layouts
 * @param shaderStages stages of the shader reads (`Barrier::INFERRED_SHADER_STAGES` => fragment shader)
 * @return `std::nullopt` -> no scope for the layout: undefined, preinitialized or not known
 * @pre `shaderStages` has only shader stages
 */
[[nodiscard]] constexpr std::optional<BarrierScope> layout_scope(
    VkImageLayout,
    VkImageAspectFlags aspects,
    VkPipelineStageFlags2 shaderStages = Barrier::INFERRED_SHADER_STAGES
);

/**
 * scopes of the transition: `src` from the old layout, `dst` from the new layout
 * @details `src` keeps only the writes of the old layout
 * @details old layout undefined, preinitialized or present -> `src` waits for the `dst` stages without access
 * @return `std::nullopt` -> no scope for a layout
 * @pre `shaderStages` has only shader stages
 */
[[nodiscard]] constexpr std::optional<LayoutTransition> layout_transition(
    VkImageLayout newLayout,
    VkImageLayout oldLayout,
    VkImageAspectFlags aspects,
    VkPipelineStageFlags2 shaderStages = Barrier::INFERRED_SHADER_STAGES
);
}

namespace cvk {
constexpr std::optional<BarrierScope> layout_scope(
    VkImageLayout layout,
    VkImageAspectFlags aspects,
    VkPipelineStageFlags2 shaderStages
) {
    constexpr VkPipelineStageFlags2 shaderStageMask = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT
        | VK_PIPELINE_STAGE_2_TESSELLATION_CONTROL_SHADER_BIT
        | VK_PIPELINE_STAGE_2_TESSELLATION_EVALUATION_SHADER_BIT
        | VK_PIPELINE_STAGE_2_GEOMETRY_SHADER_BIT
        | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT
        | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT
        | VK_PIPELINE_STAGE_2_PRE_RASTERIZATION_SHADERS_BIT
        | VK_PIPELINE_STAGE_2_TASK_SHADER_BIT_EXT
        | VK_PIPELINE_STAGE_2_MESH_SHADER_BIT_EXT
        | VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;
    CTH_CRITICAL(
        (shaderStages & ~shaderStageMask) != 0,
        "shader stages must only have shader stage bits, got {:#x}",
        shaderStages
    ) {}

    auto const shaders = shaderStages != Barrier::INFERRED_SHADER_STAGES
        ? shaderStages
        : VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    bool const color = (aspects & VK_IMAGE_ASPECT_COLOR_BIT) != 0;

    constexpr VkPipelineStageFlags2 tests = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT
        | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
    constexpr BarrierScope colorAttachment{
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT
    };
    constexpr BarrierScope depthAttachment{
        tests,
        VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
    };
    BarrierScope const shaderRead{shaders, VK_ACCESS_2_SHADER_READ_BIT};
    BarrierScope const depthRead{
        tests | shaders,
        VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_SHADER_READ_BIT
    };
    // attachments that the rendering also reads as input attachments
    constexpr auto localRead = [](BarrierScope const& attachment) {
        return BarrierScope{
            attachment.stages | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
            attachment.access | VK_ACCESS_2_INPUT_ATTACHMENT_READ_BIT
        };
    };

    switch(layout) {
        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            return BarrierScope{VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT};
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            return BarrierScope{VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT};
        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL: return shaderRead;
        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL: return colorAttachment;
        case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL:
        case VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL:
        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
        case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL:
        case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL: return depthAttachment;
        case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL:
        case VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL:
        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL: return depthRead;
        case VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL: return color ? colorAttachment : depthAttachment;
        case VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL: return color ? shaderRead : depthRead;
        case VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR: return localRead(color ? colorAttachment : depthAttachment);
        case VK_IMAGE_LAYOUT_GENERAL:
            return BarrierScope{
                VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT
            };
        case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR: return BarrierScope{};
        default: return std::nullopt;
    }
}

constexpr std::optional<LayoutTransition> layout_transition(
    VkImageLayout newLayout,
    VkImageLayout oldLayout,
    VkImageAspectFlags aspects,
    VkPipelineStageFlags2 shaderStages
) {
    auto const dst = layout_scope(newLayout, aspects, shaderStages);
    if(!dst.has_value()) return std::nullopt;

    // no content to keep: src waits for the dst stages without access,
    // orders the transition after their last use and after an acquire semaphore wait on these stages
    if(oldLayout == VK_IMAGE_LAYOUT_UNDEFINED || oldLayout == VK_IMAGE_LAYOUT_PREINITIALIZED
        || oldLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
        return LayoutTransition{.src = {dst->stages}, .dst = *dst};

    auto const src = layout_scope(oldLayout, aspects, shaderStages);
    if(!src.has_value()) return std::nullopt;

    // reads leave nothing to make visible, only their stages finish first
    constexpr VkAccessFlags2 writes = VK_ACCESS_2_TRANSFER_WRITE_BIT
        | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT
        | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
        | VK_ACCESS_2_SHADER_WRITE_BIT
        | VK_ACCESS_2_MEMORY_WRITE_BIT;
    return LayoutTransition{.src = {src->stages, src->access & writes}, .dst = *dst};
}
}
