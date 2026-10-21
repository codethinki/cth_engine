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
 * @brief one side of a barrier: stages, access and queue family
 */
struct BarrierScope {
    VkPipelineStageFlags2 stages = VK_PIPELINE_STAGE_2_NONE;
    VkAccessFlags2 access = VK_ACCESS_2_NONE;

    /**
     * @brief VK_QUEUE_FAMILY_IGNORED -> no queue family ownership transfer
     */
    queue_family_index_t queueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
};

/**
 * @brief image part of a barrier
 */
struct ImageBarrierInfo {
    BarrierScope src{};
    BarrierScope dst{};

    /**
     * @brief == @ref oldLayout -> no layout transition
     */
    VkImageLayout newLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    /**
     * @brief layout of the mip range before the barrier
     * @attention VK_IMAGE_LAYOUT_UNDEFINED discards the contents, the validation layers do not report it
     */
    VkImageLayout oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    uint32_t firstMipLevel = 0;

    /**
     * @brief VK_REMAINING_MIP_LEVELS -> to the last mip level
     */
    uint32_t mipLevels = VK_REMAINING_MIP_LEVELS;

    /**
     * @brief 0 -> the aspect mask of the image
     */
    VkImageAspectFlags aspectMask = 0;

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
     * @brief queue family ownership transfer without layout transition, dst and src name the families
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
 * @brief buffer part of a barrier
 */
struct BufferBarrierInfo {
    BarrierScope src{};
    BarrierScope dst{};

    /**
     * @brief in bytes
     */
    size_t offset = 0;

    /**
     * @brief in bytes (VK_WHOLE_SIZE => to the end)
     */
    size_t size = VK_WHOLE_SIZE;

    /**
     * @brief queue family ownership transfer, dst and src name the families
     */
    [[nodiscard]] static constexpr BufferBarrierInfo QueueTransition(BarrierScope const& dst, BarrierScope const& src) {
        return BufferBarrierInfo{.src = src, .dst = dst};
    }
};

/**
 * @brief global memory part of a barrier
 */
struct MemoryBarrierInfo {
    BarrierScope src{};
    BarrierScope dst{};
};

/**
 * @brief pipeline barrier: memory, buffer and image barriers in one vkCmdPipelineBarrier2()
 * @details aggregate, the add functions are shortcuts,
 *  @ref execute() checks the barriers
 * @attention the images and buffers do not move until @ref execute()
 */
struct Barrier {
    using MemoryInfo = MemoryBarrierInfo;
    using ImageInfo = ImageBarrierInfo;
    using BufferInfo = BufferBarrierInfo;

    /**
     * @brief shader stages of @ref addTransition(): the layout decides, fragment shader for shader reads
     */
    static constexpr VkPipelineStageFlags2 INFERRED_SHADER_STAGES = VK_PIPELINE_STAGE_2_NONE;

    struct BufferEntry {
        not_null_ptr<BaseBuffer const> buffer;
        BufferInfo info;
    };

    struct ImageEntry {
        not_null_ptr<Image const> image;
        ImageInfo info;
    };

    /**
     * @brief VK_DEPENDENCY_BY_REGION_BIT -> the dependency is per framebuffer region, e.g. per tile
     */
    VkDependencyFlags flags = 0;

    std::vector<MemoryInfo> memory{};
    std::vector<BufferEntry> buffers{};
    std::vector<ImageEntry> images{};

    /**
     * @brief adds a global memory barrier
     */
    constexpr void add(BarrierScope const& dst, BarrierScope const& src) {
        memory.push_back({.src = src, .dst = dst});
    }

    /**
     * @brief adds an image barrier
     */
    constexpr void add(Image const& image, ImageInfo const& info) { images.push_back({&image, info}); }

    /**
     * @brief adds a layout transition with the scopes of @ref layout_transition()
     * @param shaderStages stages of the shader reads in the layouts (@ref INFERRED_SHADER_STAGES => fragment shader)
     * @param mipLevels (VK_REMAINING_MIP_LEVELS => to the last mip level)
     * @pre @ref layout_scope() knows both layouts
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
     * @brief adds a buffer barrier
     */
    constexpr void add(BaseBuffer const& buffer, BufferInfo const& info) { buffers.push_back({&buffer, info}); }

    /**
     * @brief records the barriers
     * @details inside a rendering: checks the rules of barriers inside a dynamic rendering
     * @pre @ref BaseCmdBuffer::recording()
     * @pre the images and buffers are created and the ranges lie in them
     * @pre memory barriers have no queue family ownership transfer
     * @pre the new layout of a layout transition or queue family ownership transfer
     *  is not VK_IMAGE_LAYOUT_UNDEFINED or VK_IMAGE_LAYOUT_PREINITIALIZED
     * @pre inside a rendering: framebuffer space stages, VK_DEPENDENCY_BY_REGION_BIT,
     *  no layout transition, no queue family ownership transfer, attachments in local read or general layout
     */
    void execute(BaseCmdBuffer const&) const;

    /**
     * @brief removes every barrier, keeps the flags
     */
    constexpr void clear() {
        memory.clear();
        buffers.clear();
        images.clear();
    }

    [[nodiscard]] constexpr bool empty() const { return memory.empty() && buffers.empty() && images.empty(); }

private:
    /**
     * @brief checks the rules of barriers inside a dynamic rendering
     * @param rendering the command buffer began
     */
    void debug_check_rendering(
        RenderingInfo const& rendering,
        std::span<VkMemoryBarrier2 const>,
        std::span<VkBufferMemoryBarrier2 const>,
        std::span<VkImageMemoryBarrier2 const>
    ) const;
};
}

namespace cvk {
/**
 * @brief scopes of a layout transition
 */
struct LayoutTransition {
    BarrierScope src;
    BarrierScope dst;
};

/**
 * @brief what the gpu does with an image in the layout
 * @details input attachments: fragment shader only, general: all commands
 * @param aspects of the image, pick color or depth for the attachment, read only and local read layouts
 * @param shaderStages stages of the shader reads (@ref Barrier::INFERRED_SHADER_STAGES => fragment shader)
 * @return std::nullopt -> no scope for the layout: undefined, preinitialized or not known
 * @pre shaderStages has only shader stages
 */
[[nodiscard]] constexpr std::optional<BarrierScope> layout_scope(
    VkImageLayout,
    VkImageAspectFlags aspects,
    VkPipelineStageFlags2 shaderStages = Barrier::INFERRED_SHADER_STAGES
);

/**
 * @brief scopes of the transition: src from the old layout, dst from the new layout
 * @details src keeps the writes of the old layout,
 *  old layout undefined, preinitialized or present -> src waits for the dst stages without access
 * @return std::nullopt -> no scope for a layout
 * @pre shaderStages has only shader stages
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
    CTH_CRITICAL((shaderStages & ~shaderStageMask) != 0, "shader stages must only have shader stage bits") {
        details->add("shader stages: {:#x}", shaderStages);
    }

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
