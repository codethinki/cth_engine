#include "CthSubpass.hpp"

#include "CthAttachmentCollection.hpp"

namespace cth::vk {

Subpass::Subpass(
    uint32_t index,
    VkPipelineBindPoint bind_point,
    std::span<AttachmentCollection* const> input_attachments,
    std::span<AttachmentCollection* const> color_attachments,
    std::span<AttachmentCollection* const> resolve_attachments,
    AttachmentCollection const* depth_attachment,
    std::span<AttachmentCollection* const> preserve_attachments) : _index{index}, _bindPoint{bind_point} {
    auto attachmentView = []<class Rng>(Rng const& rng) {
        return rng | std::views::transform([](auto const* element) { return element->reference(); });
    };
    auto indexView = []<class Rng>(Rng const& rng) { return rng | std::views::transform([](auto const* element) { return element->index(); }); };

    auto inputAttachments = attachmentView(input_attachments);
    auto colorAttachments = attachmentView(color_attachments);
    auto resolveAttachments = attachmentView(resolve_attachments);
    auto preserveAttachments = indexView(preserve_attachments);

    _inputAttachments = {inputAttachments.begin(), inputAttachments.end()};
    _colorAttachments = {colorAttachments.begin(), colorAttachments.end()};
    _resolveAttachments = {resolveAttachments.begin(), resolveAttachments.end()};
    _depthAttachment = depth_attachment->reference();
    _preserveAttachments = {preserveAttachments.begin(), preserveAttachments.end()};

    auto attachments = ranges::concat_view(input_attachments, color_attachments, resolve_attachments);
    std::ranges::copy(attachments, std::back_inserter(_attachments));
    _attachments.emplace_back(depth_attachment);

}
VkSubpassDescription Subpass::create() const {
    return {
        .pipelineBindPoint = _bindPoint,
        .inputAttachmentCount = static_cast<uint32_t>(_inputAttachments.size()),
        .pInputAttachments = _inputAttachments.data(),
        .colorAttachmentCount = static_cast<uint32_t>(_colorAttachments.size()),
        .pColorAttachments = _colorAttachments.data(),
        .pResolveAttachments = _resolveAttachments.data(),
        .pDepthStencilAttachment = _depthAttachment ? &_depthAttachment.value() : nullptr,
        .preserveAttachmentCount = static_cast<uint32_t>(_preserveAttachments.size()),
        .pPreserveAttachments = _preserveAttachments.data(),
    };
}
Subpass Subpass::Graphics(uint32_t index,
    std::span<AttachmentCollection* const> input_attachments,
    std::span<AttachmentCollection* const> color_attachments,
    std::span<AttachmentCollection* const> resolve_attachments,
    AttachmentCollection const* depth_attachment,
    std::span<AttachmentCollection* const> const preserve_attachments
    ) {
    return Subpass{
        index,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        input_attachments,
        color_attachments,
        resolve_attachments,
        depth_attachment,
        preserve_attachments
    };
}



}
