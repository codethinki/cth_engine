#pragma once
#include "cvk/base/queue/queue_family.hpp"
#include "cvk/render/cmd/cmd_buffer.hpp"
#include "cvk/utility/format.hpp"
#include "cvk/utility/types.hpp"

#include <volk.h>

#include <cth/io/log.hpp>

#include <algorithm>
#include <array>
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
     * @brief queue family ownership transfer without layout transition, src and dst name the families
     * @param layout of the image
     */
    [[nodiscard]] static constexpr ImageBarrierInfo QueueTransition(
        VkImageLayout layout,
        BarrierScope const& src,
        BarrierScope const& dst
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
     * @brief queue family ownership transfer, src and dst name the families
     */
    [[nodiscard]] static constexpr BufferBarrierInfo QueueTransition(BarrierScope const& src, BarrierScope const& dst) {
        return BufferBarrierInfo{.src = src, .dst = dst};
    }
};

/**
 * @brief scopes of a common layout transition
 */
struct LayoutTransition {
    BarrierScope src;
    BarrierScope dst;
};

/**
 * @return the scopes of the transition, no common transition -> std::nullopt
 */
[[nodiscard]] constexpr std::optional<LayoutTransition> layout_transition(
    VkImageLayout newLayout,
    VkImageLayout oldLayout
);


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
    constexpr void add(BarrierScope const& src, BarrierScope const& dst) { memory.push_back({src, dst}); }

    /**
     * @brief adds an image barrier
     */
    constexpr void add(Image const& image, ImageInfo const& info) { images.push_back({&image, info}); }

    /**
     * @brief adds a layout transition with the scopes of @ref layout_transition()
     * @param mipLevels (VK_REMAINING_MIP_LEVELS => to the last mip level)
     * @pre @ref layout_transition() knows the transition
     */
    constexpr void addTransition(
        Image const&,
        VkImageLayout newLayout,
        VkImageLayout oldLayout,
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
constexpr void Barrier::addTransition(
    Image const& image,
    VkImageLayout newLayout,
    VkImageLayout oldLayout,
    uint32_t firstMipLevel,
    uint32_t mipLevels
) {
    auto const transition = layout_transition(newLayout, oldLayout);
    CTH_CRITICAL(!transition.has_value(), "layout transition has no table entry, use add() with scopes") {
        details->add("transition: {} -> {}", oldLayout, newLayout);
    }

    add(
        image,
        ImageInfo::LayoutTransition(newLayout, oldLayout, transition->dst, transition->src, firstMipLevel, mipLevels)
    );
}
}

namespace cvk {
constexpr std::optional<LayoutTransition> layout_transition(VkImageLayout newLayout, VkImageLayout oldLayout) {
    struct Entry {
        VkImageLayout newLayout;
        VkImageLayout oldLayout;
        LayoutTransition transition;
    };

    constexpr BarrierScope none{};
    constexpr BarrierScope transferWrite{VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT};
    constexpr BarrierScope transferRead{VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT};
    constexpr BarrierScope fragmentRead{VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT};
    constexpr BarrierScope colorWrite{
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT
    };
    constexpr BarrierScope color{
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT
    };
    constexpr BarrierScope depth{
        VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
        VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
    };
    // attachments that the rendering also reads as input attachments
    constexpr BarrierScope localRead{
        color.stages | depth.stages | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
        color.access | depth.access | VK_ACCESS_2_INPUT_ATTACHMENT_READ_BIT
    };
    constexpr BarrierScope localReadWrite{
        color.stages | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
    };

    // from VK_IMAGE_LAYOUT_UNDEFINED: src waits for the dst stages without access,
    // orders the transition after their last use and after an acquire semaphore wait on these stages
    constexpr auto stagesOf = [](BarrierScope const& scope) { return BarrierScope{scope.stages}; };

    constexpr std::array entries{
        Entry{
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_UNDEFINED,
            {stagesOf(transferWrite), transferWrite},
        },
        Entry{
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            {transferWrite, fragmentRead},
        },
        Entry{
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            {transferWrite, transferRead},
        },
        Entry{
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            {transferRead, fragmentRead},
        },

        Entry{
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_UNDEFINED,
            {stagesOf(color), color},
        },
        Entry{
            VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_UNDEFINED,
            {stagesOf(depth), depth},
        },
        Entry{
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            {colorWrite, fragmentRead},
        },
        Entry{
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            {colorWrite, none},
        },

        Entry{
            VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR,
            VK_IMAGE_LAYOUT_UNDEFINED,
            {stagesOf(localRead), localRead},
        },
        Entry{
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR,
            {localReadWrite, fragmentRead},
        },
        Entry{
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR,
            {colorWrite, none},
        },
    };

    auto const it = std::ranges::find_if(entries, [newLayout, oldLayout](Entry const& entry) {
        return entry.newLayout == newLayout && entry.oldLayout == oldLayout;
    });
    if(it == entries.end()) return std::nullopt;
    return it->transition;
}
}
