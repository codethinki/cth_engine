#include "cvk/render/cmd/cmd_buffer.hpp"

#include "cvk/base/core.hpp"
#include "cvk/data/img/image.hpp"
#include "cvk/data/img/image_view.hpp"
#include "cvk/utility/vk_exceptions.hpp"

#include <algorithm>
#include <ranges>


namespace cvk {
using std::span;

namespace {
    /**
     * @brief layout of the view from the layout tracking of its image
     */
    VkImageLayout attachment_layout(ImageView const& view) {
        auto const layout = view.image().layout(view.range().baseMipLevel);

        CTH_CRITICAL(
            layout == VK_IMAGE_LAYOUT_UNDEFINED || layout == VK_IMAGE_LAYOUT_PREINITIALIZED
            || layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL || layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
            || layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL || layout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            "attachment must be in an attachment layout"
        ) {
            details->add("layout: {}", layout);
        }

        return layout;
    }

    uint32_t view_mip_levels(ImageView const& view) {
        auto const range = view.range();
        return range.levelCount == VK_REMAINING_MIP_LEVELS ? view.image().mipLevels() - range.baseMipLevel
            : range.levelCount;
    }

    VkExtent2D view_extent(ImageView const& view) {
        auto const mip = view.range().baseMipLevel;
        auto const extent = view.image().extent();
        return {std::max(extent.width >> mip, 1u), std::max(extent.height >> mip, 1u)};
    }
}

BaseCmdBuffer::BaseCmdBuffer(CmdPool const& pool, VkCommandBufferLevel level) : _handle{pool}, _level{level} {
    CmdPool::debug_check(pool);
}

void BaseCmdBuffer::wrap(State const& state) {
    optDestroy();

    _handle.reset(state.vkBuffer.get(), pool());
    pool().add();
    _recording = false;
    _rendering.reset();
}

void BaseCmdBuffer::create() {
    CmdPool::debug_check(pool());

    optDestroy();

    VkCommandBufferAllocateInfo const info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,
        .commandPool = pool().get(),
        .level = _level,
        .commandBufferCount = 1,
    };

    VkCommandBuffer ptr = VK_NULL_HANDLE;
    auto const result = core().functions()->vkAllocateCommandBuffers(core().vkDevice(), &info, &ptr);

    CTH_STABLE_ERR(result != VK_SUCCESS, "failed to allocate command buffer") {
        forget();
        throw cvk::vk_result_exception{result, details->exception()};
    }

    _handle.reset(ptr, pool());
    pool().add();
    _recording = false;
    _rendering.reset();
}

void BaseCmdBuffer::destroy() {
    debug_check(*this);

    _handle.reset();

    forget();
}

BaseCmdBuffer::State BaseCmdBuffer::release() {
    debug_check(*this);

    State const state{_handle.release()};
    pool().remove();

    forget();
    return state;
}

void BaseCmdBuffer::end() {
    debug_check(*this);
    CTH_CRITICAL(!recording(), "command buffer must be recording") {}
    CTH_CRITICAL(rendering() != nullptr && !rendering()->inherited, "rendering must be ended first") {}

    auto const result = core().functions()->vkEndCommandBuffer(get());
    _recording = false;
    _rendering.reset();

    CVK_RESULT_STABLE_THROW(result != VK_SUCCESS, result, "failed to end command buffer") {}
}

void BaseCmdBuffer::reset(VkCommandBufferResetFlags flags) {
    debug_check(*this);
    CTH_CRITICAL(
        (pool().config().flags & VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT) == 0,
        "pool must allow command buffer resets"
    ) {}

    auto const result = core().functions()->vkResetCommandBuffer(get(), flags);
    _recording = false;
    _rendering.reset();

    CVK_RESULT_STABLE_THROW(result != VK_SUCCESS, result, "failed to reset command buffer") {}
}

void BaseCmdBuffer::beginRendering(RenderingConfig const& config) {
    debug_check(*this);
    CTH_CRITICAL(!recording(), "command buffer must be recording") {}
    CTH_CRITICAL(rendering() != nullptr, "command buffer must be outside of a rendering") {}

    Rendering rendering{
        .inheritance = {
            .viewMask = config.viewMask,
            .flags = config.flags & ~VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT,
        },
        .flags = config.flags,
    };
    std::optional<VkExtent2D> extent{};

    auto const attachmentInfo = [&](RenderingAttachment const& attachment) {
        ImageView const& view = *attachment.view;
        ImageView::debug_check(view);
        CTH_CRITICAL(view_mip_levels(view) != 1, "attachment views must have one mip level") {}

        auto const viewExtent = view_extent(view);
        extent = extent.has_value()
            ? VkExtent2D{std::min(extent->width, viewExtent.width), std::min(extent->height, viewExtent.height)}
            : viewExtent;
        rendering.inheritance.samples = view.image().config().samples;
        rendering.attachments.emplace_back(&view.image());

        VkImageView resolveView = VK_NULL_HANDLE;
        VkImageLayout resolveLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        if(attachment.resolveView != nullptr) {
            ImageView::debug_check(*attachment.resolveView);
            CTH_CRITICAL(attachment.resolveMode == VK_RESOLVE_MODE_NONE, "a resolve view needs a resolve mode") {}

            resolveView = attachment.resolveView->get();
            resolveLayout = attachment_layout(*attachment.resolveView);
            rendering.attachments.emplace_back(&attachment.resolveView->image());
        }

        return VkRenderingAttachmentInfo{
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .pNext = nullptr,
            .imageView = view.get(),
            .imageLayout = attachment_layout(view),
            .resolveMode = attachment.resolveMode,
            .resolveImageView = resolveView,
            .resolveImageLayout = resolveLayout,
            .loadOp = attachment.loadOp,
            .storeOp = attachment.storeOp,
            .clearValue = attachment.clearValue,
        };
    };

    std::vector<VkRenderingAttachmentInfo> colors{};
    colors.reserve(config.colorAttachments.size());
    for(auto const& attachment : config.colorAttachments) {
        colors.push_back(attachmentInfo(attachment));
        rendering.inheritance.colorFormats.push_back(attachment.view->image().format());
    }

    std::optional<VkRenderingAttachmentInfo> depth{};
    if(config.depthAttachment.has_value()) {
        depth = attachmentInfo(*config.depthAttachment);
        rendering.inheritance.depthFormat = config.depthAttachment->view->image().format();
    }

    std::optional<VkRenderingAttachmentInfo> stencil{};
    if(config.stencilAttachment.has_value()) {
        stencil = attachmentInfo(*config.stencilAttachment);
        rendering.inheritance.stencilFormat = config.stencilAttachment->view->image().format();
    }

    CTH_CRITICAL(!extent.has_value() && !config.area.has_value(), "a rendering without attachments needs an area") {}

    VkRenderingInfo const info{
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .pNext = nullptr,
        .flags = config.flags,
        .renderArea = config.area.value_or(VkRect2D{.offset = {0, 0}, .extent = extent.value_or(VkExtent2D{})}),
        .layerCount = 1,
        .viewMask = config.viewMask,
        .colorAttachmentCount = static_cast<uint32_t>(colors.size()),
        .pColorAttachments = colors.data(),
        .pDepthAttachment = depth.has_value() ? &*depth : nullptr,
        .pStencilAttachment = stencil.has_value() ? &*stencil : nullptr,
    };
    core().functions()->vkCmdBeginRendering(get(), &info);

    _rendering = std::move(rendering);
}

void BaseCmdBuffer::endRendering() {
    debug_check(*this);
    CTH_CRITICAL(rendering() == nullptr || rendering()->inherited, "command buffer must be inside its own rendering") {}

    core().functions()->vkCmdEndRendering(get());

    _rendering.reset();
}

void BaseCmdBuffer::setAttachmentLocations(span<uint32_t const> locations) const {
    debug_check(*this);
    CTH_CRITICAL(rendering() == nullptr, "command buffer must be inside a rendering") {}
    CTH_CRITICAL(
        locations.size() != rendering()->inheritance.colorFormats.size(),
        "one location per color attachment"
    ) {}

    VkRenderingAttachmentLocationInfoKHR const info{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_LOCATION_INFO_KHR,
        .pNext = nullptr,
        .colorAttachmentCount = static_cast<uint32_t>(locations.size()),
        .pColorAttachmentLocations = locations.data(),
    };
    core().functions()->vkCmdSetRenderingAttachmentLocationsKHR(get(), &info);
}

void BaseCmdBuffer::setInputAttachmentIndices(
    span<uint32_t const> colorIndices,
    std::optional<uint32_t> depthIndex,
    std::optional<uint32_t> stencilIndex
) const {
    debug_check(*this);
    CTH_CRITICAL(rendering() == nullptr, "command buffer must be inside a rendering") {}
    CTH_CRITICAL(
        colorIndices.size() != rendering()->inheritance.colorFormats.size(),
        "one input attachment index per color attachment"
    ) {}

    VkRenderingInputAttachmentIndexInfoKHR const info{
        .sType = VK_STRUCTURE_TYPE_RENDERING_INPUT_ATTACHMENT_INDEX_INFO_KHR,
        .pNext = nullptr,
        .colorAttachmentCount = static_cast<uint32_t>(colorIndices.size()),
        .pColorAttachmentInputIndices = colorIndices.data(),
        .pDepthInputAttachmentIndex = depthIndex.has_value() ? &*depthIndex : nullptr,
        .pStencilInputAttachmentIndex = stencilIndex.has_value() ? &*stencilIndex : nullptr,
    };
    core().functions()->vkCmdSetRenderingInputAttachmentIndicesKHR(get(), &info);
}

void BaseCmdBuffer::destroy(DeviceTable table, VkCommandPool vkPool, VkCommandBuffer vkBuffer) {
    CTH_WARN(vkBuffer == VK_NULL_HANDLE, "vkBuffer invalid (VK_NULL_HANDLE)") {}

    table->vkFreeCommandBuffers(table.device(), vkPool, 1, &vkBuffer);
}

void BaseCmdBuffer::destroy(DeviceTable table, VkCommandPool vkPool, span<VkCommandBuffer const> vkBuffers) {
    CTH_WARN(std::ranges::contains(vkBuffers, VK_NULL_HANDLE), "> 0 vkBuffers invalid (VK_NULL_HANDLE)") {}

    table->vkFreeCommandBuffers(table.device(), vkPool, static_cast<uint32_t>(vkBuffers.size()), vkBuffers.data());
}

void BaseCmdBuffer::begin(VkCommandBufferBeginInfo const& info, RenderingInheritance const* inherited) {
    debug_check(*this);
    CTH_CRITICAL(recording(), "command buffer must not be recording") {}

    auto const result = core().functions()->vkBeginCommandBuffer(get(), &info);
    CVK_RESULT_STABLE_THROW(result != VK_SUCCESS, result, "failed to begin command buffer") {}

    _recording = true;
    _beginResets = pool().resets();
    _rendering.reset();
    if(inherited != nullptr)
        _rendering = Rendering{.inheritance = *inherited, .flags = inherited->flags, .inherited = true};
}

void BaseCmdBuffer::forget() {
    _recording = false;
    _rendering.reset();
}

void BaseCmdBuffer::pool_deleter::operator()(VkCommandBuffer vkBuffer) const {
    _pool->remove();
    destroy(_pool->core().deviceTable(), _pool->get(), vkBuffer);
}
}

namespace cvk {
PrimaryCmdBuffer::PrimaryCmdBuffer(CmdPool const& pool) : BaseCmdBuffer{pool, VK_COMMAND_BUFFER_LEVEL_PRIMARY} {}

PrimaryCmdBuffer::PrimaryCmdBuffer(CmdPool const& pool, State const& state) : PrimaryCmdBuffer{pool} { wrap(state); }

PrimaryCmdBuffer::PrimaryCmdBuffer(CmdPool const& pool, create_t) : PrimaryCmdBuffer{pool} { create(); }

void PrimaryCmdBuffer::begin(VkCommandBufferUsageFlags usage) {
    VkCommandBufferBeginInfo const info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = usage,
        .pInheritanceInfo = nullptr,
    };
    BaseCmdBuffer::begin(info);
}

void PrimaryCmdBuffer::execute(span<SecondaryCmdBuffer const> secondaries) const {
    debug_check(*this);
    CTH_CRITICAL(!recording(), "command buffer must be recording") {}
    CTH_CRITICAL(
        std::ranges::any_of(
            secondaries,
            [](SecondaryCmdBuffer const& secondary) { return !secondary.created() || secondary.recording(); }
        ),
        "secondaries must be created and not recording"
    ) {}
    CTH_CRITICAL(
        rendering() != nullptr && (rendering()->flags & VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT) == 0,
        "the rendering must have VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT"
    ) {}

    std::vector const vkBuffers{std::from_range, secondaries | std::views::transform(&SecondaryCmdBuffer::get)};

    core().functions()->vkCmdExecuteCommands(get(), static_cast<uint32_t>(vkBuffers.size()), vkBuffers.data());
}
}

namespace cvk {
SecondaryCmdBuffer::SecondaryCmdBuffer(CmdPool const& pool) : BaseCmdBuffer{pool, VK_COMMAND_BUFFER_LEVEL_SECONDARY} {}

SecondaryCmdBuffer::SecondaryCmdBuffer(CmdPool const& pool, State const& state) : SecondaryCmdBuffer{pool} {
    wrap(state);
}

SecondaryCmdBuffer::SecondaryCmdBuffer(CmdPool const& pool, create_t) : SecondaryCmdBuffer{pool} { create(); }

void SecondaryCmdBuffer::begin(VkCommandBufferUsageFlags usage) {
    VkCommandBufferInheritanceInfo const inheritanceInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
        .pNext = nullptr,
        .renderPass = VK_NULL_HANDLE,
        .subpass = 0,
        .framebuffer = VK_NULL_HANDLE,
        .occlusionQueryEnable = VK_FALSE,
        .queryFlags = 0,
        .pipelineStatistics = 0,
    };
    VkCommandBufferBeginInfo const info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = usage,
        .pInheritanceInfo = &inheritanceInfo,
    };
    BaseCmdBuffer::begin(info);
}

void SecondaryCmdBuffer::begin(Inheritance const& inheritance, VkCommandBufferUsageFlags usage) {
    auto const renderingInfo = inheritance.renderingInfo();
    VkCommandBufferInheritanceInfo const inheritanceInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
        .pNext = &renderingInfo,
        .renderPass = VK_NULL_HANDLE,
        .subpass = 0,
        .framebuffer = VK_NULL_HANDLE,
        .occlusionQueryEnable = VK_FALSE,
        .queryFlags = 0,
        .pipelineStatistics = 0,
    };
    VkCommandBufferBeginInfo const info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = usage | VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT,
        .pInheritanceInfo = &inheritanceInfo,
    };
    BaseCmdBuffer::begin(info, &inheritance);
}
}
