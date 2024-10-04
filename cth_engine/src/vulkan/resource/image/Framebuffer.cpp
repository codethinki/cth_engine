#include "Framebuffer.hpp"

#include "CthImageView.hpp"
#include "../CthDestructionQueue.hpp"
#include "vulkan/base/CthCore.hpp"
#include "vulkan/base/CthDevice.hpp"
#include "vulkan/render/pass/CthAttachmentCollection.hpp"
#include "vulkan/render/pass/CthRenderPass.hpp"
#include "vulkan/utility/cth_vk_exceptions.hpp"

namespace cth::vk {

Framebuffer::Framebuffer(cth::not_null<Core const*> core, cth::not_null<RenderPass const*> render_pass, std::span<ImageView const* const> attachments,
    uint32_t layers) : _core{core}, _renderPass{render_pass}, _attachments{std::from_range, attachments}, _layers{layers} {}
Framebuffer::Framebuffer(cth::not_null<Core const*> core, cth::not_null<RenderPass const*> render_pass, std::span<ImageView const* const> attachments,
    State const& state, uint32_t layers) : Framebuffer{core, render_pass, attachments, layers} { wrap(state); }
Framebuffer::Framebuffer(cth::not_null<Core const*> core, cth::not_null<RenderPass const*> render_pass, std::span<ImageView const* const> attachments,
    VkExtent2D extent, uint32_t layers) : Framebuffer{core, render_pass, attachments, layers} { create(extent); }


Framebuffer::~Framebuffer() { optDestroy(); }

void Framebuffer::wrap(State const& state) {
    optDestroy();
    _handle = state.vkFramebuffer.get();
    _extent = state.extent;
}

void Framebuffer::create(VkExtent2D extent) {
    optDestroy();

    _extent = extent;

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

    auto const createResult = vkCreateFramebuffer(_core->vkDevice(), &createInfo, nullptr, &ptr);

    CTH_STABLE_ERR(createResult != VK_SUCCESS, "failed to create framebuffer") {
        reset();
        throw cth::vk::result_exception{createResult, details->exception()};
    }

    _handle = ptr;
}
void Framebuffer::destroy() {
    debug_check(this);
    auto const lambda = [vk_device = _core->vkDevice(), vk_framebuffer = _handle.get()]() { destroy(vk_device, vk_framebuffer); };

    auto const queue = _core->destructionQueue();

    if(queue) queue->push(lambda);
    else lambda();

    //TEMP use reset();
    _handle = nullptr;
}
Framebuffer::State Framebuffer::release() {
    debug_check(this);

    State const state{
        _handle.release(),
        _extent,
    };
    reset();
    return state;
}
void Framebuffer::destroy(vk::not_null<VkDevice> vk_device, VkFramebuffer vk_framebuffer) {
    DEBUG_CHECK_DEVICE_HANDLE(vk_device);
    CTH_WARN(vk_framebuffer == VK_NULL_HANDLE, "framebuffer should not be invalid (VK_NULL_HANDLE") {}

    vkDestroyFramebuffer(vk_device.get(), vk_framebuffer, nullptr);
}
void Framebuffer::reset() {
    _handle = VK_NULL_HANDLE;
    _extent = {};
}

}
