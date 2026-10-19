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
 * @brief maps the attachments of a rendering to the fragment shader outputs and input attachments
 * @details a draw requires the same mapping in the bound pipeline and in the command buffer
 */
struct AttachmentMapping {
    /**
     * @brief fragment shader output location per color attachment (VK_ATTACHMENT_UNUSED => no output)
     * @details empty -> color attachment i writes location i
     */
    std::vector<uint32_t> locations{};

    /**
     * @brief input attachment index per color attachment (VK_ATTACHMENT_UNUSED => no input attachment)
     * @details empty -> color attachment i is input attachment i
     */
    std::vector<uint32_t> colorInputIndices{};

    /**
     * @brief std::nullopt -> the depth attachment is the input attachment without index
     */
    std::optional<uint32_t> depthInputIndex = std::nullopt;

    /**
     * @brief std::nullopt -> the stencil attachment is the input attachment without index
     */
    std::optional<uint32_t> stencilInputIndex = std::nullopt;

    /**
     * @param colorAttachments of the rendering
     * @pre @ref debug_check() passes for colorAttachments
     * @attention pColorAttachmentLocations points into @ref locations
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
     * @pre @ref debug_check() passes for colorAttachments
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
     * @brief same mapping, empty == the default
     */
    [[nodiscard]] constexpr bool operator==(AttachmentMapping const&) const;

    /**
     * @param colorAttachments of the rendering
     */
    static constexpr void debug_check(AttachmentMapping const&, size_t colorAttachments);
};

/**
 * @brief formats, samples, view mask, flags and attachment mapping of a rendering, what a secondary inherits from it
 */
struct RenderingInheritance {
    std::vector<VkFormat> colorFormats{};
    VkFormat depthFormat = VK_FORMAT_UNDEFINED;
    VkFormat stencilFormat = VK_FORMAT_UNDEFINED;
    VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
    uint32_t viewMask = 0;
    VkRenderingFlags flags = 0;

    /**
     * @brief mapping at the begin of the secondary
     * @pre == the mapping of the primary at the execute
     */
    AttachmentMapping attachmentMapping{};

    /**
     * @attention pColorAttachmentFormats points into @ref colorFormats
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
 * @brief attachment of a rendering
 */
struct RenderingAttachment {
    /**
     * @pre one mip level
     */
    not_null_ptr<ImageView const> view;

    /**
     * @brief layout of the view during the rendering, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL fits every aspect
     */
    VkImageLayout layout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;

    VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    VkAttachmentStoreOp storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    /**
     * @brief VK_ATTACHMENT_LOAD_OP_CLEAR -> the clear value
     */
    VkClearValue clearValue{};

    /**
     * @brief nullptr -> no resolve
     */
    ImageView const* resolveView = nullptr;

    /**
     * @brief layout of @ref resolveView during the rendering
     */
    VkImageLayout resolveLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;

    /**
     * @pre != VK_RESOLVE_MODE_NONE if @ref resolveView
     */
    VkResolveModeFlagBits resolveMode = VK_RESOLVE_MODE_NONE;
};

/**
 * @brief attachments, area, flags and attachment mapping of a dynamic rendering
 */
struct RenderingInfo {
    std::vector<RenderingAttachment> colorAttachments{};
    std::optional<RenderingAttachment> depthAttachment = std::nullopt;
    std::optional<RenderingAttachment> stencilAttachment = std::nullopt;

    /**
     * @brief std::nullopt -> the extent that fits every attachment
     */
    std::optional<VkRect2D> area = std::nullopt;

    uint32_t viewMask = 0;

    /**
     * @brief VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT -> secondaries record the rendering
     */
    VkRenderingFlags flags = 0;

    /**
     * @brief mapping at the begin, @ref BaseCmdBuffer::setAttachmentMapping() changes it inside the rendering
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
        "one location per color attachment"
    ) {
        details->add("locations: {}, color attachments: {}", mapping.locations.size(), colorAttachments);
    }
    CTH_CRITICAL(
        !mapping.colorInputIndices.empty() && mapping.colorInputIndices.size() != colorAttachments,
        "one input attachment index per color attachment"
    ) {
        details->add("indices: {}, color attachments: {}", mapping.colorInputIndices.size(), colorAttachments);
    }
}
}
