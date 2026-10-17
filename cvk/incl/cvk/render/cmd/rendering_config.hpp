#pragma once
#include "cvk/utility/types.hpp"

#include <volk.h>

#include <optional>
#include <vector>


namespace cvk {
class ImageView;

/**
 * @brief formats, samples and view mask of a rendering, what a secondary command buffer inherits
 */
struct RenderingInheritance {
    std::vector<VkFormat> colorFormats{};
    VkFormat depthFormat = VK_FORMAT_UNDEFINED;
    VkFormat stencilFormat = VK_FORMAT_UNDEFINED;
    VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
    uint32_t viewMask = 0;
    VkRenderingFlags flags = 0;

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
    not_null<ImageView const*> view;

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
     * @pre != VK_RESOLVE_MODE_NONE if @ref resolveView
     */
    VkResolveModeFlagBits resolveMode = VK_RESOLVE_MODE_NONE;
};

/**
 * @brief attachments, area and flags of a dynamic rendering
 */
struct RenderingConfig {
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
};
}
