#pragma once
#include "vulkan/utility/cth_constants.hpp"


namespace cth::vk {
class AttachmentCollection;
class Subpass {
public:
    Subpass(uint32_t index, VkPipelineBindPoint bind_point,
        std::span<AttachmentCollection* const> input_attachments = {},
        std::span<AttachmentCollection* const> color_attachments = {},
        std::span<AttachmentCollection* const> resolve_attachments = {},
        AttachmentCollection const* depth_attachment = nullptr,
        std::span<AttachmentCollection* const> preserve_attachments = {}
        );

    [[nodiscard]] VkSubpassDescription create() const;

private:
    uint32_t _index;
    VkPipelineBindPoint _bindPoint;

    std::vector<VkAttachmentReference> _inputAttachments;
    std::vector<VkAttachmentReference> _colorAttachments;
    std::vector<VkAttachmentReference> _resolveAttachments;
    std::optional<VkAttachmentReference> _depthAttachment;

    std::vector<uint32_t> _preserveAttachments;

    std::vector<AttachmentCollection const*> _attachments;

public:
    [[nodiscard]] uint32_t index() const { return _index; }
    [[nodiscard]] std::span<AttachmentCollection const* const> attachments() const { return _attachments; }


    static Subpass Graphics(
        uint32_t index,
        std::span<AttachmentCollection* const> input_attachments = {},
        std::span<AttachmentCollection* const> color_attachments = {},
        std::span<AttachmentCollection* const> resolve_attachments = {},
        AttachmentCollection const* depth_attachment = nullptr,
        std::span<AttachmentCollection* const> preserve_attachments = {}
        );

    static void debug_check(Subpass const* subpass);
    static void debug_check(std::span<Subpass const* const> subpasses);
};

}


namespace cth::vk {
inline void Subpass::debug_check(Subpass const* subpass) {
    CTH_ERR(subpass == nullptr, "subpass must not be invalid (nullptr)") throw details->exception();
}
inline void Subpass::debug_check(std::span<Subpass const* const> subpasses) {
    for(auto const* subpass : subpasses) debug_check(subpass);
}
}
