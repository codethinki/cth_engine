#include "cvk/render/cmd/cmd_buffer.hpp"

#include "cvk/base/core.hpp"
#include "cvk/data/descriptor/descriptor_set.hpp"
#include "cvk/data/img/image.hpp"
#include "cvk/data/img/image_format.hpp"
#include "cvk/data/img/image_view.hpp"
#include "cvk/render/pipeline/layout/pipeline_layout.hpp"
#include "cvk/render/pipeline/pipeline.hpp"
#include "cvk/utility/vk_exceptions.hpp"

#include <algorithm>
#include <ranges>


namespace cvk {
using std::span;

namespace {
    void debug_check_attachment_layout(VkImageLayout layout) {
        CTH_CRITICAL(
            layout == VK_IMAGE_LAYOUT_UNDEFINED || layout == VK_IMAGE_LAYOUT_PREINITIALIZED
            || layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL || layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
            || layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL || layout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            "attachment must be in an attachment layout"
        ) {
            details->add("layout: {}", layout);
        }
    }

    constexpr bool dynamic_buffer(VkDescriptorType type) {
        return type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC || type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
    }

    uint32_t view_mip_levels(ImageView const& view) {
        auto const range = view.range();
        return range.levelCount == VK_REMAINING_MIP_LEVELS ? view.image().mipLevels() - range.baseMipLevel
            : range.levelCount;
    }

    void debug_check_draw(BaseCmdBuffer const& cmdBuffer) {
        BaseCmdBuffer::debug_check(cmdBuffer);

        // a secondary can be inside the rendering of its primary
        auto const* rendering = cmdBuffer.rendering();
        CTH_CRITICAL(
            rendering == nullptr && cmdBuffer.level() == VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            "draws need a rendering"
        ) {}
        CTH_CRITICAL(
            rendering != nullptr && (rendering->flags & VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT) != 0
            && (rendering->flags & VK_RENDERING_CONTENTS_INLINE_BIT_KHR) == 0,
            "the secondaries record the draws of a rendering with VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT"
        ) {}
    }

    void debug_check_indirect(
        BaseBuffer const& buffer,
        size_t offset,
        uint32_t drawCount,
        uint32_t stride,
        size_t size
    ) {
        BaseBuffer::debug_check(buffer);
        CTH_CRITICAL(
            (buffer.config().usage & VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT) == 0,
            "buffer usage must have VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT"
        ) {}
        CTH_CRITICAL(offset % 4 != 0, "offset must be a multiple of 4") {}
        CTH_CRITICAL(
            drawCount > 0 && offset + size_t{drawCount - 1} * stride + size > buffer.size(),
            "the commands must lie in the buffer"
        ) {}
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
    CTH_CRITICAL(rendering() != nullptr, "rendering must be ended first") {}

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

void BaseCmdBuffer::beginRendering(RenderingInfo const& info) {
    debug_check(*this);
    CTH_CRITICAL(!recording(), "command buffer must be recording") {}
    CTH_CRITICAL(rendering() != nullptr, "command buffer must be outside of a rendering") {}
    AttachmentMapping::debug_check(info.attachmentMapping, info.colorAttachments.size());

    std::optional<VkExtent2D> extent{};

    auto const attachmentInfo = [&](RenderingAttachment const& attachment) {
        ImageView const& view = *attachment.view;
        ImageView::debug_check(view);
        CTH_CRITICAL(view_mip_levels(view) != 1, "attachment views must have one mip level") {}

        auto const viewExtent = view_extent(view);
        extent = extent.has_value()
            ? VkExtent2D{std::min(extent->width, viewExtent.width), std::min(extent->height, viewExtent.height)}
            : viewExtent;
        debug_check_attachment_layout(attachment.layout);

        VkImageView resolveView = VK_NULL_HANDLE;
        VkImageLayout resolveLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        if(attachment.resolveView != nullptr) {
            ImageView::debug_check(*attachment.resolveView);
            CTH_CRITICAL(attachment.resolveMode == VK_RESOLVE_MODE_NONE, "a resolve view needs a resolve mode") {}

            debug_check_attachment_layout(attachment.resolveLayout);

            resolveView = attachment.resolveView->get();
            resolveLayout = attachment.resolveLayout;
        }

        return VkRenderingAttachmentInfo{
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .pNext = nullptr,
            .imageView = view.get(),
            .imageLayout = attachment.layout,
            .resolveMode = attachment.resolveMode,
            .resolveImageView = resolveView,
            .resolveImageLayout = resolveLayout,
            .loadOp = attachment.loadOp,
            .storeOp = attachment.storeOp,
            .clearValue = attachment.clearValue,
        };
    };

    std::vector<VkRenderingAttachmentInfo> colors{};
    colors.reserve(info.colorAttachments.size());
    for(auto const& attachment : info.colorAttachments) {
        colors.push_back(attachmentInfo(attachment));

        auto const format = attachment.view->image().format();
        CTH_CRITICAL(format_aspects(format) != VK_IMAGE_ASPECT_COLOR_BIT, "color attachments need a color format") {
            details->add("format: {}", format);
        }
    }

    std::optional<VkRenderingAttachmentInfo> depth{};
    if(info.depthAttachment.has_value()) {
        depth = attachmentInfo(*info.depthAttachment);

        auto const format = info.depthAttachment->view->image().format();
        CTH_CRITICAL(
            (format_aspects(format) & VK_IMAGE_ASPECT_DEPTH_BIT) == 0,
            "the depth attachment needs a depth format"
        ) {
            details->add("format: {}", format);
        }
    }

    std::optional<VkRenderingAttachmentInfo> stencil{};
    if(info.stencilAttachment.has_value()) {
        stencil = attachmentInfo(*info.stencilAttachment);

        auto const format = info.stencilAttachment->view->image().format();
        CTH_CRITICAL(
            (format_aspects(format) & VK_IMAGE_ASPECT_STENCIL_BIT) == 0,
            "the stencil attachment needs a stencil format"
        ) {
            details->add("format: {}", format);
        }
    }

    CTH_CRITICAL(!extent.has_value() && !info.area.has_value(), "a rendering without attachments needs an area") {}

    VkRenderingInfo const renderingInfo{
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .pNext = nullptr,
        .flags = info.flags,
        .renderArea = info.area.value_or(VkRect2D{.offset = {0, 0}, .extent = extent.value_or(VkExtent2D{})}),
        .layerCount = 1,
        .viewMask = info.viewMask,
        .colorAttachmentCount = static_cast<uint32_t>(colors.size()),
        .pColorAttachments = colors.data(),
        .pDepthAttachment = depth.has_value() ? &*depth : nullptr,
        .pStencilAttachment = stencil.has_value() ? &*stencil : nullptr,
    };
    core().functions()->vkCmdBeginRendering(get(), &renderingInfo);

    _rendering = info;
    _rendering->area = renderingInfo.renderArea;
    if(_rendering->attachmentMapping != AttachmentMapping{}) recordAttachmentMapping();

    // secondaries do not inherit dynamic state
    bool const inlineContents = (info.flags & VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT) == 0
        || (info.flags & VK_RENDERING_CONTENTS_INLINE_BIT_KHR) != 0;
    if(!inlineContents) return;

    auto const& area = renderingInfo.renderArea;
    setViewport(
        VkViewport{
            .x = static_cast<float>(area.offset.x),
            .y = static_cast<float>(area.offset.y),
            .width = static_cast<float>(area.extent.width),
            .height = static_cast<float>(area.extent.height),
            .minDepth = 0.f,
            .maxDepth = 1.f,
        }
    );
    setScissor(area);
}

void BaseCmdBuffer::endRendering() {
    debug_check(*this);
    CTH_CRITICAL(rendering() == nullptr, "command buffer must be inside its own rendering") {}

    core().functions()->vkCmdEndRendering(get());

    _rendering.reset();
}

void BaseCmdBuffer::setAttachmentMapping(AttachmentMapping const& mapping) {
    debug_check(*this);
    CTH_CRITICAL(rendering() == nullptr, "command buffer must be inside its own rendering") {}
    AttachmentMapping::debug_check(mapping, _rendering->colorAttachments.size());

    _rendering->attachmentMapping = mapping;
    recordAttachmentMapping();
}

void BaseCmdBuffer::setViewport(VkViewport const& viewport) const {
    debug_check(*this);
    CTH_CRITICAL(!recording(), "command buffer must be recording") {}

    core().functions()->vkCmdSetViewport(get(), 0, 1, &viewport);
}

void BaseCmdBuffer::setScissor(VkRect2D const& scissor) const {
    debug_check(*this);
    CTH_CRITICAL(!recording(), "command buffer must be recording") {}

    core().functions()->vkCmdSetScissor(get(), 0, 1, &scissor);
}

void BaseCmdBuffer::bindPipeline(Pipeline const& pipeline) {
    debug_check(*this);
    CTH_CRITICAL(!recording(), "command buffer must be recording") {}
    Pipeline::debug_check(pipeline);

    auto const& mapping = pipeline.config().attachmentMapping;
    if(rendering() != nullptr && mapping != rendering()->attachmentMapping) setAttachmentMapping(mapping);

    core().functions()->vkCmdBindPipeline(get(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.get());
}

void BaseCmdBuffer::bindDescriptorSet(
    DescriptorSet const& set,
    PipelineLayout const& layout,
    uint32_t setIndex,
    VkPipelineBindPoint bindPoint,
    span<uint32_t const> dynamicOffsets
) const {
    debug_check(*this);
    CTH_CRITICAL(!recording(), "command buffer must be recording") {}
    DescriptorSet::debug_check(set);
    PipelineLayout::debug_check(layout);
    CTH_CRITICAL(setIndex >= layout.setLayouts().size(), "the pipeline layout has no set {}", setIndex) {}

    auto const dynamicCount = std::ranges::fold_left(
        set.layout().bindings(),
        uint32_t{0},
        [](uint32_t sum, VkDescriptorSetLayoutBinding const& binding) {
            return dynamic_buffer(binding.descriptorType) ? sum + binding.descriptorCount : sum;
        }
    );
    CTH_CRITICAL(dynamicOffsets.size() != dynamicCount, "one dynamic offset per dynamic buffer descriptor") {
        details->add("dynamic descriptors: {}, offsets: {}", dynamicCount, dynamicOffsets.size());
    }

    VkDescriptorSet const vkSet = set.get();
    core().functions()->vkCmdBindDescriptorSets(
        get(),
        bindPoint,
        layout.get(),
        setIndex,
        1,
        &vkSet,
        static_cast<uint32_t>(dynamicOffsets.size()),
        dynamicOffsets.data()
    );
}

void BaseCmdBuffer::pushConstantBytes(
    PipelineLayout const& layout,
    VkShaderStageFlags stages,
    span<std::byte const> data,
    uint32_t offset
) const {
    debug_check(*this);
    CTH_CRITICAL(!recording(), "command buffer must be recording") {}
    PipelineLayout::debug_check(layout);
    CTH_CRITICAL(data.empty(), "push constants must not be empty") {}
    CTH_CRITICAL(offset % 4 != 0 || data.size() % 4 != 0, "offset and size must be multiples of 4") {}

    core().functions()->vkCmdPushConstants(
        get(),
        layout.get(),
        stages,
        offset,
        static_cast<uint32_t>(data.size()),
        data.data()
    );
}

void BaseCmdBuffer::bindVertexBuffer(uint32_t binding, BaseBuffer const& buffer, size_t offset) const {
    debug_check(*this);
    BaseBuffer::debug_check(buffer);
    CTH_CRITICAL(!recording(), "command buffer must be recording") {}
    CTH_CRITICAL(
        (buffer.config().usage & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT) == 0,
        "buffer usage must have VK_BUFFER_USAGE_VERTEX_BUFFER_BIT"
    ) {}
    CTH_CRITICAL(offset >= buffer.size(), "offset must lie in the buffer") {}

    VkBuffer const vkBuffer = buffer.get();
    VkDeviceSize const vkOffset = offset;
    core().functions()->vkCmdBindVertexBuffers(get(), binding, 1, &vkBuffer, &vkOffset);
}

void BaseCmdBuffer::bindIndexBuffer(BaseBuffer const& buffer, VkIndexType type, size_t offset) const {
    debug_check(*this);
    BaseBuffer::debug_check(buffer);
    CTH_CRITICAL(!recording(), "command buffer must be recording") {}
    CTH_CRITICAL(
        (buffer.config().usage & VK_BUFFER_USAGE_INDEX_BUFFER_BIT) == 0,
        "buffer usage must have VK_BUFFER_USAGE_INDEX_BUFFER_BIT"
    ) {}
    CTH_CRITICAL(offset >= buffer.size(), "offset must lie in the buffer") {}
    CTH_CRITICAL(
        offset % (type == VK_INDEX_TYPE_UINT16 ? sizeof(uint16_t) : sizeof(uint32_t)) != 0,
        "offset must be a multiple of the index size"
    ) {}

    core().functions()->vkCmdBindIndexBuffer(get(), buffer.get(), offset, type);
}

void BaseCmdBuffer::draw(
    uint32_t vertexCount,
    uint32_t instanceCount,
    uint32_t firstVertex,
    uint32_t firstInstance
) const {
    debug_check_draw(*this);

    core().functions()->vkCmdDraw(get(), vertexCount, instanceCount, firstVertex, firstInstance);
}

void BaseCmdBuffer::drawIndexed(
    uint32_t indexCount,
    uint32_t instanceCount,
    uint32_t firstIndex,
    int32_t vertexOffset,
    uint32_t firstInstance
) const {
    debug_check_draw(*this);

    core().functions()->vkCmdDrawIndexed(get(), indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

void BaseCmdBuffer::drawIndirect(BaseBuffer const& buffer, size_t offset, uint32_t drawCount, uint32_t stride) const {
    debug_check_draw(*this);
    debug_check_indirect(buffer, offset, drawCount, stride, sizeof(VkDrawIndirectCommand));

    core().functions()->vkCmdDrawIndirect(get(), buffer.get(), offset, drawCount, stride);
}

void BaseCmdBuffer::drawIndexedIndirect(
    BaseBuffer const& buffer,
    size_t offset,
    uint32_t drawCount,
    uint32_t stride
) const {
    debug_check_draw(*this);
    debug_check_indirect(buffer, offset, drawCount, stride, sizeof(VkDrawIndexedIndirectCommand));

    core().functions()->vkCmdDrawIndexedIndirect(get(), buffer.get(), offset, drawCount, stride);
}

void BaseCmdBuffer::destroy(DeviceTable table, VkCommandPool vkPool, VkCommandBuffer vkBuffer) {
    CTH_WARN(vkBuffer == VK_NULL_HANDLE, "vkBuffer invalid (VK_NULL_HANDLE)") {}

    table->vkFreeCommandBuffers(table.device(), vkPool, 1, &vkBuffer);
}

void BaseCmdBuffer::destroy(DeviceTable table, VkCommandPool vkPool, span<VkCommandBuffer const> vkBuffers) {
    CTH_WARN(std::ranges::contains(vkBuffers, VK_NULL_HANDLE), "> 0 vkBuffers invalid (VK_NULL_HANDLE)") {}

    table->vkFreeCommandBuffers(table.device(), vkPool, static_cast<uint32_t>(vkBuffers.size()), vkBuffers.data());
}

void BaseCmdBuffer::begin(VkCommandBufferBeginInfo const& info) {
    debug_check(*this);
    CTH_CRITICAL(recording(), "command buffer must not be recording") {}

    auto const result = core().functions()->vkBeginCommandBuffer(get(), &info);
    CVK_RESULT_STABLE_THROW(result != VK_SUCCESS, result, "failed to begin command buffer") {}

    _recording = true;
    _beginResets = pool().resets();
    _rendering.reset();
}

void BaseCmdBuffer::recordAttachmentMapping() const {
    auto const& mapping = _rendering->attachmentMapping;
    auto const colorAttachments = static_cast<uint32_t>(_rendering->colorAttachments.size());

    auto const locations = mapping.locationInfo(colorAttachments);
    auto const inputIndices = mapping.inputIndexInfo(colorAttachments);
    core().functions()->vkCmdSetRenderingAttachmentLocationsKHR(get(), &locations);
    core().functions()->vkCmdSetRenderingInputAttachmentIndicesKHR(get(), &inputIndices);
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

RenderingInheritance PrimaryCmdBuffer::inheritance() const {
    debug_check(*this);
    CTH_CRITICAL(rendering() == nullptr, "command buffer must be inside its own rendering") {}

    auto const& info = *rendering();
    auto const format = [](RenderingAttachment const& attachment) { return attachment.view->image().format(); };

    // every attachment has the same sample count
    auto const samples = [&info] {
        auto const of = [](RenderingAttachment const& attachment) { return attachment.view->image().config().samples; };
        if(!info.colorAttachments.empty()) return of(info.colorAttachments.front());
        if(info.depthAttachment.has_value()) return of(*info.depthAttachment);
        if(info.stencilAttachment.has_value()) return of(*info.stencilAttachment);
        return VK_SAMPLE_COUNT_1_BIT;
    };

    return RenderingInheritance{
        .colorFormats = {std::from_range, info.colorAttachments | std::views::transform(format)},
        .depthFormat = info.depthAttachment.has_value() ? format(*info.depthAttachment) : VK_FORMAT_UNDEFINED,
        .stencilFormat = info.stencilAttachment.has_value() ? format(*info.stencilAttachment) : VK_FORMAT_UNDEFINED,
        .samples = samples(),
        .viewMask = info.viewMask,
        .flags = info.flags & ~VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT,
        .attachmentMapping = info.attachmentMapping,
    };
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
    auto const colorAttachments = static_cast<uint32_t>(inheritance.colorFormats.size());
    AttachmentMapping::debug_check(inheritance.attachmentMapping, colorAttachments);

    auto const inputIndices = inheritance.attachmentMapping.inputIndexInfo(colorAttachments);
    auto locations = inheritance.attachmentMapping.locationInfo(colorAttachments);
    locations.pNext = &inputIndices;
    auto renderingInfo = inheritance.renderingInfo();
    renderingInfo.pNext = &locations;

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
    BaseCmdBuffer::begin(info);
}
}
