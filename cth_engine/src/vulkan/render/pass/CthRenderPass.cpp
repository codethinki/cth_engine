#include "CthRenderPass.hpp"

#include "CthAttachmentCollection.hpp"
#include "CthSubpass.hpp"

#include "../cmd/CthCmdBuffer.hpp"

#include "vulkan/base/CthCore.hpp"
#include "vulkan/base/CthDevice.hpp"
#include "vulkan/resource/CthDestructionQueue.hpp"
#include "vulkan/resource/image/Framebuffer.hpp"
#include "vulkan/utility/cth_vk_exceptions.hpp"

namespace cth::vk {



RenderPass::RenderPass(cth::not_null<Core const*> core, std::span<Subpass const* const> subpasses, std::span<VkSubpassDependency const> dependencies,
    std::span<BeginConfig const> begin_configs) : _core{core}, _subpasses{std::from_range, subpasses},
    _dependencies{std::from_range, dependencies} {

    Core::debug_check(core);
    Subpass::debug_check(_subpasses);

    initAttachments();

    for(auto [clearValues, extent, subpassContents, offset] : begin_configs) {
        _clearValues.insert_range(_clearValues.end(), clearValues);
        _contents.push_back(subpassContents);

        _beginInfos.emplace_back(
            VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            nullptr,
            _handle.get(),
            VK_NULL_HANDLE,
            VkRect2D{offset, extent},
            static_cast<uint32_t>(clearValues.size()),
            &_clearValues[_clearValues.size() - clearValues.size()]
            );
    }

}
RenderPass::RenderPass(cth::not_null<Core const*> core, std::span<Subpass const* const> subpasses, std::span<VkSubpassDependency const> dependencies,
    std::span<BeginConfig const> begin_configs, State const& state) : RenderPass{core, subpasses, dependencies, begin_configs} { wrap(state); }

RenderPass::RenderPass(cth::not_null<Core const*> core, std::span<Subpass const* const> subpasses,
    std::span<VkSubpassDependency const> dependencies, std::span<BeginConfig const> begin_configs,
    bool create) : RenderPass{core, subpasses, dependencies, begin_configs} { if(create) this->create(); }

RenderPass::~RenderPass() { optDestroy(); }

void RenderPass::wrap(State const& state) {
    optDestroy();

    _handle = state.vkRenderPass.get();
}
void RenderPass::create() {
    optDestroy();


    std::vector<VkSubpassDescription> subpasses{_subpasses.size()};
    std::ranges::transform(_subpasses, subpasses.begin(), [](Subpass const* subpass) { return subpass->create(); });

    std::vector<VkAttachmentDescription> attachments{_attachments.size()};
    std::ranges::transform(_attachments, attachments.begin(), [](AttachmentCollection const* attachment) { return attachment->description(); });


    VkRenderPassCreateInfo const createInfo{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .attachmentCount = static_cast<uint32_t>(attachments.size()),
        .pAttachments = attachments.data(),
        .subpassCount = static_cast<uint32_t>(subpasses.size()),
        .pSubpasses = subpasses.data(),
        .dependencyCount = static_cast<uint32_t>(_dependencies.size()),
        .pDependencies = _dependencies.data()
    };

    VkRenderPass ptr = VK_NULL_HANDLE;
    auto const result = vkCreateRenderPass(_core->vkDevice(), &createInfo, nullptr, &ptr);

    CTH_STABLE_ERR(result != VK_SUCCESS, "failed to create render pass")
        throw cth::vk::result_exception{result, details->exception()};

    _handle = ptr;

    for(auto& beginInfo : _beginInfos) beginInfo.renderPass = _handle.get();
}
void RenderPass::destroy() {
    debug_check(this);

    auto const lambda = [vk_device = _core->vkDevice(), handle = _handle.get()] { destroy(vk_device, handle); };

    auto const queue = _core->destructionQueue();
    if(queue) queue->push(lambda);
    else lambda();

    reset();
}


RenderPass::State RenderPass::release() {
    debug_check(this);

    State const state{
        .vkRenderPass = _handle.get()
    };

    reset();
    return state;
}
void RenderPass::begin(cth::not_null<PrimaryCmdBuffer const*> cmd_buffer, uint32_t config_index, cth::not_null<Framebuffer const*> framebuffer) {
    CmdBuffer::debug_check(cmd_buffer);
    Framebuffer::debug_check(framebuffer);
    CTH_CRITICAL(config_index >= _beginInfos.size(), "config_index out of range") {}

    _beginInfos[config_index].framebuffer = framebuffer->get();

    vkCmdBeginRenderPass(cmd_buffer->get(), &_beginInfos[config_index], _contents[config_index]);
}
void RenderPass::end(cth::not_null<PrimaryCmdBuffer const*> cmd_buffer) { vkCmdEndRenderPass(cmd_buffer->get()); }

void RenderPass::destroy(vk::not_null<VkDevice> vk_device, VkRenderPass vk_render_pass) {
    CTH_WARN(vk_render_pass == VK_NULL_HANDLE, "vk_render_pass should not be invalid (VK_NULL_HANDLE)") {}

    vkDestroyRenderPass(vk_device.get(), vk_render_pass, nullptr);
}

void RenderPass::reset() {
    _handle = nullptr;
    for(auto& beginInfo : _beginInfos) beginInfo.renderPass = VK_NULL_HANDLE;
}

void debug_check_attachments(std::span<AttachmentCollection const* const> attachments) {
    CTH_CRITICAL(std::ranges::any_of(attachments | std::views::enumerate, [](std::tuple<ptrdiff_t, AttachmentCollection const*>const& pair){
        return static_cast<uint32_t>(std::get<0>(pair)) != std:: get<1>(pair)->index();}), "invalid attachments or indices submitted in subpasses") {
        uint32_t i = 0;
        std::vector<uint32_t> missingIndices{};
        for(auto const* attachment : attachments) {
            if(attachment->index() != i) {
                missingIndices.push_back(i);
                i = attachment->index();
            }
            ++i;
        }
        details->add("missing indices: {}", missingIndices);
        throw details->exception();
    }
}

void RenderPass::initAttachments() {
    _attachments = _subpasses | std::views::transform([](Subpass const* subpass) { return subpass->attachments(); })
        | std::views::join | std::ranges::to<std::vector<AttachmentCollection const*>>();

    std::ranges::sort(_attachments, [](AttachmentCollection const* a, AttachmentCollection const* b) { return a->index() < b->index(); });
    auto const duplicates = std::ranges::unique(_attachments);


    _attachments.erase(std::ranges::begin(duplicates), std::ranges::end(duplicates));

    debug_check_attachments(_attachments);
}



}
