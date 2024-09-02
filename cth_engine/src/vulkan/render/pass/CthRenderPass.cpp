#include "CthRenderPass.hpp"

#include "CthAttachmentCollection.hpp"
#include "CthSubpass.hpp"

#include "../cmd/CthCmdBuffer.hpp"

#include "vulkan/base/CthCore.hpp"
#include "vulkan/base/CthDevice.hpp"
#include "vulkan/resource/CthDestructionQueue.hpp"
#include "vulkan/resource/image/Framebuffer.hpp"
#include "vulkan/utility/cth_vk_utils.hpp"

namespace cth::vk {
RenderPass::RenderPass(not_null<BasicCore const*> core, std::span<Subpass const* const> subpasses,
    std::span<VkSubpassDependency const> dependencies, std::span<BeginConfig const> begin_configs,
    bool create) : _core{core}, _subpasses{std::from_range, subpasses},
    _dependencies{std::from_range, dependencies} {
    DEBUG_CHECK_CORE(core);

    DEBUG_CHECK_SUBPASSES(_subpasses);

    _attachments = subpasses | std::views::transform([](Subpass const* subpass) { return subpass->attachments(); })
        | std::views::join | std::ranges::to<std::vector<AttachmentCollection const*>>();

    std::ranges::sort(_attachments, [](AttachmentCollection const* a, AttachmentCollection const* b) { return a->index() < b->index(); });
    auto const duplicates = std::ranges::unique(_attachments);


    _attachments.erase(std::ranges::begin(duplicates), std::ranges::end(duplicates));

    CTH_ERR(std::ranges::any_of(_attachments | std::views::enumerate, [](std::tuple<ptrdiff_t, AttachmentCollection const*>const& pair){
        return static_cast<uint32_t>(std::get<0>(pair)) != std:: get<1>(pair)->index();}), "invalid attachments or indices submitted in subpasses") {
        uint32_t i = 0;
        std::vector<uint32_t> missingIndices{};
        for(auto const* attachment : _attachments) {
            if(attachment->index() != i) {
                missingIndices.push_back(i);
                i = attachment->index();
            }
            ++i;
        }
        details->add("missing indices: {}", missingIndices);
        throw details->exception();
    }

    for(auto [clearValues, extent, subpassContents, offset] : begin_configs) {
        _clearValues.insert_range(_clearValues.end(), clearValues);
        _contents.push_back(subpassContents);

        _beginInfos.emplace_back(
            VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
            nullptr,
            _handle.get(),
            VK_NULL_HANDLE,
            VkRect2D{offset, extent},
            static_cast<uint32_t>(clearValues.size()),
            &_clearValues[_clearValues.size() - clearValues.size()]
            );
    }


    if(create) this->create();
}
RenderPass::~RenderPass() { if(valid()) destroy(); }
void RenderPass::wrap(gsl::owner<VkRenderPass> render_pass) {
    if(valid()) destroy();

    _handle = render_pass;
}
void RenderPass::create() {
    if(valid()) destroy();


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
}
void RenderPass::destroy() {
    CTH_ERR(!valid(), "render pass was already destroyed") throw details->exception();
    if(!valid()) [[unlikely]] return;


    if(_core->destructionQueue()) _core->destructionQueue()->push(_handle.get());
    else destroy(_core->vkDevice(), _handle.get());

    _handle = nullptr;
}


gsl::owner<VkRenderPass> RenderPass::release() {
    auto const handle = _handle.get();
    _handle = nullptr;
    return handle;
}
void RenderPass::begin(PrimaryCmdBuffer const* cmd_buffer, uint32_t config_index, Framebuffer const* framebuffer) {
    DEBUG_CHECK_CMD_BUFFER(cmd_buffer);
    //TEMP DEBUG_CHECK_FRAMEBUFFER(framebuffer);
    CTH_ERR(config_index >= _beginInfos.size(), "config_index out of range") throw details->exception();

    _beginInfos[config_index].framebuffer = framebuffer->get();

    vkCmdBeginRenderPass(cmd_buffer->get(), &_beginInfos[config_index], _contents[config_index]);
}
void RenderPass::end(PrimaryCmdBuffer const* cmd_buffer) { vkCmdEndRenderPass(cmd_buffer->get()); }

void RenderPass::destroy(VkDevice vk_device, VkRenderPass vk_render_pass) {
    DEBUG_CHECK_DEVICE_HANDLE(vk_device);
    CTH_WARN(vk_render_pass == VK_NULL_HANDLE, "vk_render_pass should not be invalid (VK_NULL_HANDLE)") {}

    vkDestroyRenderPass(vk_device, vk_render_pass, nullptr);
}


#ifdef CONSTANT_DEBUG_MODE
void RenderPass::debug_check(RenderPass const* render_pass) {
    CTH_ERR(render_pass == nullptr, "render pass must not be invalid (nullptr)") throw details->exception();
    DEBUG_CHECK_RENDER_PASS_HANDLE(render_pass->get());
}
void RenderPass::debug_check_handle(VkRenderPass vk_render_pass) {
    CTH_ERR(vk_render_pass == VK_NULL_HANDLE, "vk_render_pass must not be invalid (VK_NULL_HANDLE)") throw details->exception();
}
#endif

}
