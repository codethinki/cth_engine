#pragma once
#include "cvk/utility/types.hpp"

#include <volk.h>

#include <cth/io/log.hpp>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <ranges>
#include <vector>


namespace cvk {
class ImageView;

/**
 * attachment mapping of a rendering
 * @details maps the attachments to the fragment shader outputs and input attachments
 * @details a draw needs the same mapping in the bound pipeline and in the command buffer
 */
struct AttachmentMapping {
    /**
     * fragment shader output location per color attachment (`VK_ATTACHMENT_UNUSED` => no output)
     * @details empty -> color attachment i writes location i
     */
    std::vector<uint32_t> locations{};

    /**
     * input attachment index per color attachment (`VK_ATTACHMENT_UNUSED` => no input attachment)
     * @details empty -> color attachment i is input attachment i
     */
    std::vector<uint32_t> colorInputIndices{};

    /**
     * `std::nullopt` -> the depth attachment is the input attachment without index
     */
    std::optional<uint32_t> depthInputIndex = std::nullopt;

    /**
     * `std::nullopt` -> the stencil attachment is the input attachment without index
     */
    std::optional<uint32_t> stencilInputIndex = std::nullopt;

    /**
     * @param colorAttachments of the rendering
     * @pre `debug_check()` passes for `colorAttachments`
     * @attention `pColorAttachmentLocations` points into `locations`
     */
    [[nodiscard]] constexpr VkRenderingAttachmentLocationInfoKHR locationInfo(uint32_t colorAttachments) const {
        return VkRenderingAttachmentLocationInfoKHR{
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_LOCATION_INFO_KHR,
            .pNext = nullptr,
            .colorAttachmentCount = colorAttachments,
            .pColorAttachmentLocations = locations.empty() ? nullptr : locations.data(),
        };
    }

    /**
     * @param colorAttachments of the rendering
     * @pre `debug_check()` passes for `colorAttachments`
     * @attention the pointers point into this mapping
     */
    [[nodiscard]] constexpr VkRenderingInputAttachmentIndexInfoKHR inputIndexInfo(uint32_t colorAttachments) const {
        return VkRenderingInputAttachmentIndexInfoKHR{
            .sType = VK_STRUCTURE_TYPE_RENDERING_INPUT_ATTACHMENT_INDEX_INFO_KHR,
            .pNext = nullptr,
            .colorAttachmentCount = colorAttachments,
            .pColorAttachmentInputIndices = colorInputIndices.empty() ? nullptr : colorInputIndices.data(),
            .pDepthInputAttachmentIndex = depthInputIndex.has_value() ? &*depthInputIndex : nullptr,
            .pStencilInputAttachmentIndex = stencilInputIndex.has_value() ? &*stencilInputIndex : nullptr,
        };
    }

    /**
     * same mapping, empty == the default
     */
    [[nodiscard]] constexpr bool operator==(AttachmentMapping const&) const;

    /**
     * @param colorAttachments of the rendering
     */
    static constexpr void debug_check(AttachmentMapping const&, size_t colorAttachments);
};

/**
 * what a secondary inherits from a rendering
 */
struct RenderingInheritance {
    std::vector<VkFormat> colorFormats{};
    VkFormat depthFormat = VK_FORMAT_UNDEFINED;
    VkFormat stencilFormat = VK_FORMAT_UNDEFINED;
    VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
    uint32_t viewMask = 0;
    VkRenderingFlags flags = 0;

    /**
     * mapping at the begin of the secondary
     * @pre `==` the mapping of the primary at the execute
     */
    AttachmentMapping attachmentMapping{};

    /**
     * @attention `pColorAttachmentFormats` points into `colorFormats`
     */
    [[nodiscard]] constexpr VkCommandBufferInheritanceRenderingInfo renderingInfo() const {
        return VkCommandBufferInheritanceRenderingInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDERING_INFO,
            .pNext = nullptr,
            .flags = flags,
            .viewMask = viewMask,
            .colorAttachmentCount = static_cast<uint32_t>(colorFormats.size()),
            .pColorAttachmentFormats = colorFormats.data(),
            .depthAttachmentFormat = depthFormat,
            .stencilAttachmentFormat = stencilFormat,
            .rasterizationSamples = samples,
        };
    }
};

/**
 * attachment of a rendering
 */
struct RenderingAttachment {
    /**
     * @pre one mip level
     */
    not_null_ptr<ImageView const> view;

    /**
     * layout of the view during the rendering, `VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL` fits every aspect
     */
    VkImageLayout layout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;

    VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    VkAttachmentStoreOp storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    /**
     * `VK_ATTACHMENT_LOAD_OP_CLEAR` -> the clear value
     */
    VkClearValue clearValue{};

    /**
     * `nullptr` -> no resolve
     */
    ImageView const* resolveView = nullptr;

    /**
     * layout of `resolveView` during the rendering
     */
    VkImageLayout resolveLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;

    /**
     * @pre `!= VK_RESOLVE_MODE_NONE` if `resolveView`
     */
    VkResolveModeFlagBits resolveMode = VK_RESOLVE_MODE_NONE;
};

/**
 * dynamic rendering info
 */
struct RenderingInfo {
    std::vector<RenderingAttachment> colorAttachments{};
    std::optional<RenderingAttachment> depthAttachment = std::nullopt;
    std::optional<RenderingAttachment> stencilAttachment = std::nullopt;

    /**
     * `std::nullopt` -> the extent that fits every attachment
     */
    std::optional<VkRect2D> area = std::nullopt;

    /**
     * layers that the rendering writes, `viewMask != 0` -> ignored
     * @pre the attachment views have the layers
     */
    uint32_t layerCount = 1;

    /**
     * multiview, bit i -> view i renders into layer i
     * @pre the attachment views have a layer per view
     */
    uint32_t viewMask = 0;

    /**
     * `VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT` -> secondaries record the rendering
     */
    VkRenderingFlags flags = 0;

    /**
     * mapping at the begin, `BaseCmdBuffer::setAttachmentMapping()` changes it inside the rendering
     */
    AttachmentMapping attachmentMapping{};
};
}

namespace cvk {
constexpr bool AttachmentMapping::operator==(AttachmentMapping const& other) const {
    auto const same = [](std::vector<uint32_t> const& left, std::vector<uint32_t> const& right) {
        // the default maps attachment i to i
        auto const identity = [](std::vector<uint32_t> const& indices) {
            return std::ranges::equal(indices, std::views::iota(uint32_t{0}, static_cast<uint32_t>(indices.size())));
        };
        if(left.empty()) return identity(right);
        if(right.empty()) return identity(left);
        return left == right;
    };
    return same(locations, other.locations) && same(colorInputIndices, other.colorInputIndices)
        && depthInputIndex == other.depthInputIndex && stencilInputIndex == other.stencilInputIndex;
}

constexpr void AttachmentMapping::debug_check(AttachmentMapping const& mapping, size_t colorAttachments) {
    CTH_CRITICAL(
        !mapping.locations.empty() && mapping.locations.size() != colorAttachments,
        "one location per color attachment, got {} for {}",
        mapping.locations.size(),
        colorAttachments
    ) {}
    CTH_CRITICAL(
        !mapping.colorInputIndices.empty() && mapping.colorInputIndices.size() != colorAttachments,
        "one input attachment index per color attachment, got {} for {}",
        mapping.colorInputIndices.size(),
        colorAttachments
    ) {}
}
}
