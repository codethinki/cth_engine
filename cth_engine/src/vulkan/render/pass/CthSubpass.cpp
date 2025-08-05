#include "CthSubpass.hpp"

#include "attachment/AttachmentCollection.hpp"

namespace cth::vk {
namespace {
    auto attachment_ref_view(auto const& rng) {
        return rng | std::views::transform([](auto const* collection) { return collection->references(); }) | std::views::join;
    }
    auto index_view(auto const& rng) {
        return rng | std::views::transform([](auto const* collection) { return collection->indices(); }) | std::views::join;
    }
}


Subpass::Subpass(
    uint32_t index,
    VkPipelineBindPoint bind_point,
    std::span<AttachmentCollection const* const> color_attachments,
    std::span<AttachmentCollection const* const> resolve_attachments,
    AttachmentCollection const* depth_attachment,
    std::span<AttachmentCollection const* const> input_attachments,
    std::span<AttachmentCollection* const> preserve_attachments) :
    _index{index}, _bindPoint{bind_point} {

    auto inputAttachments = attachment_ref_view(input_attachments);
    auto colorAttachments = attachment_ref_view(color_attachments);
    auto resolveAttachments = attachment_ref_view(resolve_attachments);
    auto preserveAttachments = index_view(preserve_attachments);

    _inputAttachments = {std::from_range, inputAttachments};
    _colorAttachments = {std::from_range, colorAttachments};
    _resolveAttachments = {std::from_range, resolveAttachments};
    _preserveAttachments = {std::from_range, preserveAttachments};

    if(depth_attachment != nullptr) setDepthAttachment(*depth_attachment);


    auto attachments = ::ranges::views::concat(input_attachments, color_attachments, resolve_attachments);
    std::ranges::copy(attachments, std::back_inserter(_attachments));
    if(_depthAttachment) _attachments.emplace_back(depth_attachment);

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
void Subpass::setDepthAttachment(AttachmentCollection const& attachment) {
    auto const depthAttCount = attachment.attachments();
    CTH_CRITICAL(depthAttCount > 1, "there must at most be one depth attachment, currently: [{}]", depthAttCount) {}
    if(depthAttCount > 0) _depthAttachment = attachment.references()[0];
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
        color_attachments,
        resolve_attachments,
        depth_attachment,
        input_attachments,
        preserve_attachments
    };
}



}
