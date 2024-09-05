#include "Framebuffer.hpp"

#include "../CthDestructionQueue.hpp"

#include "vulkan/base/CthCore.hpp"
#include "vulkan/render/pass/CthAttachmentCollection.hpp"
#include "vulkan/render/pass/CthRenderPass.hpp"
#include "vulkan/utility/cth_vk_utils.hpp"

namespace cth::vk {

Framebuffer::Framebuffer(cth::not_null<Core const*> core, RenderPass const* render_pass, VkExtent2D extent,
    std::span<ImageView const* const> attachments, uint32_t layers, bool create) : _core{core}, _renderPass{render_pass}, _extent{extent},
    _attachments{std::from_range, attachments}, _layers{layers} { if(create) this->create(); }
Framebuffer::~Framebuffer() { optDestroy(); }

void Framebuffer::wrap(gsl::owner<VkFramebuffer> framebuffer) {
    optDestroy();
    _handle = framebuffer;
}
void Framebuffer::create() {
    optDestroy();

    std::vector<VkImageView> attachments{_attachments.size()};
    std::ranges::transform(_attachments, attachments.begin(), [](ImageView const* attachment) { return attachment->get(); });

    VkFramebufferCreateInfo const createInfo{
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .renderPass = _renderPass->get(),
        .attachmentCount = static_cast<uint32_t>(attachments.size()),
        .pAttachments = attachments.data(),
        .width = _extent.width,
        .height = _extent.height,
        .layers = 1,
    };

    VkFramebuffer ptr = VK_NULL_HANDLE;

    VkResult const createResult = vkCreateFramebuffer(_core->vkDevice(), &createInfo, nullptr, &ptr);

    CTH_STABLE_ERR(createResult != VK_SUCCESS, "Vk: failed to create framebuffer")
        throw cth::vk::result_exception{createResult, details->exception()};

    _handle = ptr;
}
void Framebuffer::destroy() {
    // ReSharper disable once CppTooWideScope
    auto const queue = _core->destructionQueue();

    if(queue) queue->push(_handle.get());
    else destroy(_core->vkDevice(), _handle.get());

    _handle = nullptr;
}
gsl::owner<VkFramebuffer> Framebuffer::release() {
    return _handle.release();
}
void Framebuffer::destroy(VkDevice vk_device, VkFramebuffer vk_framebuffer) {
    DEBUG_CHECK_DEVICE_HANDLE(vk_device);
    CTH_WARN(vk_framebuffer == VK_NULL_HANDLE, "framebuffer should not be invalid (VK_NULL_HANDLE") {}

    vkDestroyFramebuffer(vk_device, vk_framebuffer, nullptr);
}
}
