#pragma once
#include "vulkan/utility/cth_constants.hpp"

#include <gsl/pointers>


namespace cth::vk {
class PrimaryCmdBuffer;
class AttachmentCollection;
class Subpass;
class Core;
class Framebuffer;

class RenderPass {
public:
    struct BeginConfig;

    RenderPass(cth::not_null<Core const*> core, std::span<Subpass const* const> subpasses,
        std::span<VkSubpassDependency const> dependencies, std::span<BeginConfig const> begin_configs,
        bool create = true);
    ~RenderPass();

    void wrap(gsl::owner<VkRenderPass> render_pass);
    void create();
    void destroy();

    gsl::owner<VkRenderPass> release();

    void begin(cth::not_null<PrimaryCmdBuffer const*> cmd_buffer, uint32_t config_index, cth::not_null<Framebuffer const*> framebuffer);
    void end(cth::not_null<PrimaryCmdBuffer const*> cmd_buffer);

    static void destroy(VkDevice vk_device, VkRenderPass vk_render_pass);

private:
    cth::not_null<Core const*> _core;

    cth::move_ptr<VkRenderPass_T> _handle;

    std::vector<Subpass const*> _subpasses;
    std::vector<AttachmentCollection const*> _attachments;
    std::vector<VkSubpassDependency> _dependencies;
    std::vector<VkRenderPassBeginInfo> _beginInfos;
    std::vector<VkSubpassContents> _contents;
    std::vector<VkClearValue> _clearValues;

public:
    [[nodiscard]] VkRenderPass get() const { return _handle.get(); }
    [[nodiscard]] bool valid() const { return _handle.get() != VK_NULL_HANDLE; }

    RenderPass(RenderPass const& other) = delete;
    RenderPass(RenderPass&& other) noexcept = default;
    RenderPass& operator=(RenderPass const& other) = delete;
    RenderPass& operator=(RenderPass&& other) noexcept = default;
#ifdef CONSTANT_DEBUG_MODE
    static void debug_check(RenderPass const* render_pass);
    static void debug_check_handle(VkRenderPass vk_render_pass);


#define DEBUG_CHECK_RENDER_PASS(render_pass) RenderPass::debug_check(render_pass)
#define DEBUG_CHECK_RENDER_PASS_HANDLE(vk_render_pass) RenderPass::debug_check_handle(vk_render_pass)
#else
#define DEBUG_CHECK_RENDER_PASS(render_pass) ((void)0)
#define DEBUG_CHECK_RENDER_PASS_HANDLE(vk_render_pass) ((void)0)
#endif
};


//BeginConfig
struct RenderPass::BeginConfig {
    std::span<VkClearValue const> clearValues;
    VkExtent2D extent;
    VkSubpassContents subpassContents = VK_SUBPASS_CONTENTS_INLINE;
    VkOffset2D offset{0, 0};
};

}
