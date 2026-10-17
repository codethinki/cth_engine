#pragma once
#include "cvk/base/queue/queue_family.hpp"
#include "cvk/render/cmd/cmd_buffer.hpp"
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
        BarrierScope const& src,
        BarrierScope const& dst,
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
    VkImageLayout oldLayout,
    VkImageLayout newLayout
);


/**
 * @brief pipeline barrier: memory, buffer and image barriers in one vkCmdPipelineBarrier2()
 * @attention the images and buffers do not move until @ref execute()
 */
class Barrier {
public:
    using ImageInfo = ImageBarrierInfo;
    using BufferInfo = BufferBarrierInfo;

    /**
     * @param flags VK_DEPENDENCY_BY_REGION_BIT -> the dependency is per framebuffer region, e.g. per tile
     */
    constexpr explicit Barrier(VkDependencyFlags flags = 0) : _flags{flags} {}

    /**
     * @brief adds a global memory barrier
     * @pre no queue family ownership transfer
     */
    constexpr void add(BarrierScope const& src, BarrierScope const& dst);

    /**
     * @brief adds an image barrier
     * @pre @ref Image::created()
     * @pre the mip range lies in the image
     */
    void add(Image const&, ImageInfo const&);

    /**
     * @brief adds a layout transition with the scopes of @ref layout_transition()
     * @param mipLevels (VK_REMAINING_MIP_LEVELS => to the last mip level)
     * @pre @ref layout_transition() knows the transition
     * @pre the preconditions of @ref add(Image const&, ImageInfo const&)
     */
    void addTransition(
        Image const&,
        VkImageLayout newLayout,
        VkImageLayout oldLayout,
        uint32_t firstMipLevel = 0,
        uint32_t mipLevels = VK_REMAINING_MIP_LEVELS
    );

    /**
     * @brief adds a buffer barrier
     * @details the buffer handle comes from the buffer at @ref execute()
     * @pre @ref BaseBuffer::created()
     * @pre the range lies in the buffer
     */
    void add(BaseBuffer const&, BufferInfo const&);

    /**
     * @brief records the barriers
     * @details inside a rendering: checks the rules of barriers inside a dynamic rendering
     * @pre @ref BaseCmdBuffer::recording()
     * @pre the new layout of a layout transition or queue family ownership transfer
     *  is not VK_IMAGE_LAYOUT_UNDEFINED or VK_IMAGE_LAYOUT_PREINITIALIZED
     * @pre inside a rendering: framebuffer space stages, VK_DEPENDENCY_BY_REGION_BIT,
     *  no layout transition, no queue family ownership transfer, attachments in local read or general layout
     */
    void execute(BaseCmdBuffer const&) const;

    /**
     * @brief removes every barrier
     */
    constexpr void clear() {
        _memoryBarriers.clear();
        _buffers.clear();
        _images.clear();
    }

    [[nodiscard]] constexpr VkDependencyFlags flags() const { return _flags; }
    [[nodiscard]] constexpr bool empty() const {
        return _memoryBarriers.empty() && _buffers.empty() && _images.empty();
    }

private:
    /**
     * @brief checks the rules of barriers inside a dynamic rendering
     */
    void debug_check_rendering(
        BaseCmdBuffer::Rendering const&,
        std::span<VkBufferMemoryBarrier2 const>,
        std::span<VkImageMemoryBarrier2 const>
    ) const;

    struct BufferEntry {
        not_null<BaseBuffer const*> buffer;
        BufferInfo info;
    };

    struct ImageEntry {
        not_null<Image const*> image;
        ImageInfo info;
    };

    VkDependencyFlags _flags;
    std::vector<VkMemoryBarrier2> _memoryBarriers{};
    std::vector<BufferEntry> _buffers{};
    std::vector<ImageEntry> _images{};
};
}

namespace cvk {
constexpr void Barrier::add(BarrierScope const& src, BarrierScope const& dst) {
    CTH_CRITICAL(
        src.queueFamilyIndex != VK_QUEUE_FAMILY_IGNORED || dst.queueFamilyIndex != VK_QUEUE_FAMILY_IGNORED,
        "memory barriers have no queue family ownership transfer"
    ) {}

    _memoryBarriers.push_back(
        VkMemoryBarrier2{
            .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
            .pNext = nullptr,
            .srcStageMask = src.stages,
            .srcAccessMask = src.access,
            .dstStageMask = dst.stages,
            .dstAccessMask = dst.access,
        }
    );
}
}

namespace cvk {
constexpr std::optional<LayoutTransition> layout_transition(VkImageLayout oldLayout, VkImageLayout newLayout) {
    struct Entry {
        VkImageLayout oldLayout;
        VkImageLayout newLayout;
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

    constexpr std::array entries{
        Entry{
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            {none, transferWrite},
        },
        Entry{
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            {transferWrite, fragmentRead},
        },
        Entry{
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            {transferWrite, transferRead},
        },
        Entry{
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            {transferRead, fragmentRead},
        },

        Entry{
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            {none, color},
        },
        Entry{
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
            {none, depth},
        },
        Entry{
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            {colorWrite, fragmentRead},
        },
        Entry{
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            {colorWrite, none},
        },

        Entry{
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR,
            {none, localRead},
        },
        Entry{
            VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            {localReadWrite, fragmentRead},
        },
        Entry{
            VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR,
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            {colorWrite, none},
        },
    };

    auto const it = std::ranges::find_if(entries, [oldLayout, newLayout](Entry const& entry) {
        return entry.oldLayout == oldLayout && entry.newLayout == newLayout;
    });
    if(it == entries.end()) return std::nullopt;
    return it->transition;
}
}
