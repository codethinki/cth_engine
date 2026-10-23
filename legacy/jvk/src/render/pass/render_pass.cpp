#include "jvk/render/pass/render_pass.hpp"

#include "jvk/base/core.hpp"
#include "jvk/base/destruction_queue.hpp"
#include "jvk/render/cmd/cmd_buffer.hpp"
#include "jvk/render/pass/render_pass_config.hpp"
#include "jvk/render/pass/subpass.hpp"
#include "jvk/render/pass/attachment/attachment_collection.hpp"
#include "jvk/render/pass/framebuffer/framebuffer.hpp"
#include "jvk/utility/vk_exceptions.hpp"

#include <map>

namespace jvk {


RenderPass::RenderPass(Core const& core, Config const& config) : _core{&core},
    _subpasses{config.subpasses},
    _dependencies{config.dependencies} {
    Core::debug_check(core);
    Subpass::debug_check(_subpasses);

    initAttachments();

    auto const& [contents, clearValues, extent, offset] = config.beginConfig;
    relocateSubpassContents(contents);
    recolor(clearValues);
    resize(extent, offset);
}

RenderPass::RenderPass(
    Core const& core,
    Config const& config,
    State const& state
) : RenderPass{core, config} { wrap(state); }

RenderPass::RenderPass(Core const& core, Config const& config, create_t) : RenderPass{core, config} { create(); }

RenderPass::~RenderPass() { optDestroy(); }

void RenderPass::wrap(State const& state) {
    optDestroy();

    _handle = state.vkRenderPass.get();
}

void RenderPass::create() {
    optDestroy();


    std::vector<VkSubpassDescription> subpasses{_subpasses.size()};
    std::ranges::transform(
        _subpasses,
        subpasses.begin(),
        [](Subpass const* subpass) { return subpass->create(); }
    );

    auto attachments = attachmentDescriptions();


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
    auto const result = _core->functions()->vkCreateRenderPass(_core->vkDevice(), &createInfo, nullptr, &ptr);

    CTH_STABLE_ERR(result != VK_SUCCESS, "failed to create render pass")
    throw jvk::vk_result_exception{result, details->exception()};

    setHandle(ptr);
}

void RenderPass::destroy() {
    debug_check(*this);

    auto const lambda = [table = _core->deviceTable(), handle = _handle.get()] { destroy(table, handle); };

    auto const queue = _core->destructionQueue();
    if(queue) queue->push(lambda);
    else lambda();

    reset();
}


RenderPass::State RenderPass::release() {
    debug_check(*this);

    State const state{
        .vkRenderPass = _handle.get()
    };

    reset();
    return state;
}

void RenderPass::resize(VkExtent2D extent, VkOffset2D offset) { resize({offset, extent}); }

void RenderPass::resize(VkRect2D area) { _beginInfo.renderArea = area; }

void RenderPass::recolor(std::span<VkClearValue const> clear_values) {
    size_t index = 0;
    for(size_t i = 0; i < _attachments.size(); i++)
        if(_attachments[i]->description().stencilLoadOp == VK_ATTACHMENT_LOAD_OP_CLEAR)
            index = i;
    CTH_CRITICAL(clear_values.size() <= index, "clear values must be >= index of last cleared attachment") {}

    _clearValue.clear();
    _clearValue.insert_range(_clearValue.end(), clear_values);

    _beginInfo.clearValueCount = static_cast<uint32_t>(_clearValue.size());
    _beginInfo.pClearValues = _clearValue.data();
}

void RenderPass::relocateSubpassContents(VkSubpassContents contents) { _subpassContents = contents; }


void RenderPass::begin(PrimaryCmdBuffer const& cmd_buffer, Framebuffer const& framebuffer) {
    CmdBuffer::debug_check(cmd_buffer);
    Framebuffer::debug_check(framebuffer);
    debug_check(*this);

    _beginInfo.framebuffer = framebuffer.get();

    _core->functions()->vkCmdBeginRenderPass(cmd_buffer.get(), &_beginInfo, _subpassContents);
}

void RenderPass::end(PrimaryCmdBuffer const& cmd_buffer) { _core->functions()->vkCmdEndRenderPass(cmd_buffer.get()); }


void RenderPass::destroy(DeviceTable table, VkRenderPass vk_render_pass) {
    CTH_WARN(vk_render_pass == VK_NULL_HANDLE, "vk_render_pass should not be invalid (VK_NULL_HANDLE)") {}

    table->vkDestroyRenderPass(table.device(), vk_render_pass, nullptr);
}

void RenderPass::reset() { setHandle(nullptr); }

void RenderPass::setHandle(VkRenderPass handle) {
    _handle = handle;
    _beginInfo.renderPass = handle;
}

std::vector<VkAttachmentDescription> RenderPass::attachmentDescriptions() const {
    std::map<uint32_t, VkAttachmentDescription> map{};

    for(auto const& collection : _attachments) {
        auto description = collection->description();
        for(auto const index : collection->indices())
            map.emplace(index, description);
    }
    return {std::from_range, map | std::views::values};
}

namespace {
    void debug_check_attachments(std::span<AttachmentCollection const* const> attachments) {
        std::unordered_map<uint32_t, AttachmentCollection const*> indexMap{};
        for(auto& collection : attachments)
            for(auto index : collection->indices()) {
                auto& parent = indexMap[index];
                CTH_CRITICAL(parent != nullptr && parent != collection, "index overlap between collections") {
                    details->add("1. indices: {}", parent->indices());
                    details->add("2. indices: {}", collection->indices());
                }
                parent = collection;
            }


        std::vector indices{std::from_range, indexMap | std::views::keys};
        std::ranges::sort(indices);

        CTH_CRITICAL(
            std::ranges::any_of(
                indices | std::views::enumerate,
                [](std::tuple<ptrdiff_t, uint32_t> const& pair) {
                return std::cmp_not_equal(std::get < 0 > (pair), std::get < 1 > (pair));
                }),
            "invalid indices submitted in subpasses, must fill [0 : n-1]"
        ) { details->add("indices: {}", indices); }
    }

}

void RenderPass::initAttachments() {
    _attachments = {
        std::from_range,
        _subpasses | std::views::transform([](Subpass const* subpass) { return subpass->attachments(); }) |
        std::views::join
    };


    debug_check_attachments(_attachments);
}



}
