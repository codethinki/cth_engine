#pragma once
#include <gsl/pointers>

namespace cth::vk {
class ImageView;
}

namespace cth::vk {
class BasicCore;
class RenderPass;
class AttachmentCollection;
class Framebuffer {
public:
    Framebuffer(cth::not_null<BasicCore const*> core, RenderPass const* render_pass, VkExtent2D extent, std::span<ImageView const* const> attachments,
        uint32_t layers = 1, bool create = true);
    ~Framebuffer();

    void wrap(gsl::owner<VkFramebuffer> framebuffer);
    void create();

    void destroy();
    void optDestroy() { if(created()) destroy(); }

    gsl::owner<VkFramebuffer> release();

    static void destroy(VkDevice vk_device, VkFramebuffer vk_framebuffer);

private:
    cth::not_null<BasicCore const*> _core;
    cth::move_ptr<VkFramebuffer_T> _handle = VK_NULL_HANDLE;

    RenderPass const* _renderPass;
    VkExtent2D _extent;
    std::vector<ImageView const*> _attachments;
    uint32_t _layers;

public:
    [[nodiscard]] VkFramebuffer get() const { return _handle.get(); }
    [[nodiscard]] bool created() const { return _handle != VK_NULL_HANDLE; }

};
}
