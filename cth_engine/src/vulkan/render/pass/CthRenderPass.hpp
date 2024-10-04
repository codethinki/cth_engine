#pragma once
#include "vulkan/utility/cth_constants.hpp"
#include "vulkan/utility/cth_vk_types.hpp"

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
    struct State;

    /**
     * @brief base constructor
     */
    RenderPass(cth::not_null<Core const*> core, std::span<Subpass const* const> subpasses,
        std::span<VkSubpassDependency const> dependencies, std::span<BeginConfig const> begin_configs);

    /**
     * @brief constructs and calls @ref wrap(State const&)
     * @note calls @ref RenderPass(cth::not_null<Core const*>, std::span<Subpass const* const>, std::span<VkSubpassDependency const>, std::span<BeginConfig const>)
     */
    RenderPass(cth::not_null<Core const*> core, std::span<Subpass const* const> subpasses,
        std::span<VkSubpassDependency const> dependencies, std::span<BeginConfig const> begin_configs, State const& state);

    /**
     * @brief constructs and if(create) calls @ref create()
     * @note calls @ref RenderPass(cth::not_null<Core const*>, std::span<Subpass const* const>, std::span<VkSubpassDependency const>, std::span<BeginConfig const>)
     */
    RenderPass(cth::not_null<Core const*> core, std::span<Subpass const* const> subpasses,
        std::span<VkSubpassDependency const> dependencies, std::span<BeginConfig const> begin_configs,
        bool create);


    /**
     * @brief calls @ref optDestroy()
     */
    ~RenderPass();

    /**
     * @brief wraps the @ref State
     * @note calls @ref optDestroy()
     */
    void wrap(State const& state);

    /**
     * @brief creates the vk_render_pass
     * @throws cth::vk::result_exception result of vkCreateRenderPass
     * @note calls @ref optDestroy()
     */
    void create();


    /**
     * @brief destroys and resets the objects
     * @attention requires @ref created()
     */
    void destroy();

    /**
     * @brief if @ref created() calls @ref destroy()
     */
    void optDestroy() { if(created()) destroy(); }

    /**
     * @brief releases ownership and resets the object
     * @attention requires @ref created()
     */
    State release();

    void begin(cth::not_null<PrimaryCmdBuffer const*> cmd_buffer, uint32_t config_index, cth::not_null<Framebuffer const*> framebuffer);
    void end(cth::not_null<PrimaryCmdBuffer const*> cmd_buffer);

    static void destroy(vk::not_null<VkDevice> vk_device, VkRenderPass vk_render_pass);

private:
    void reset();

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
    [[nodiscard]] bool created() const { return _handle.get() != VK_NULL_HANDLE; }

    RenderPass(RenderPass const& other) = delete;
    RenderPass(RenderPass&& other) noexcept = default;
    RenderPass& operator=(RenderPass const& other) = delete;
    RenderPass& operator=(RenderPass&& other) noexcept = default;


    static void debug_check(RenderPass const* render_pass);
    static void debug_check_handle(VkRenderPass vk_render_pass);
};


}

//BeginConfig

namespace cth::vk {
struct RenderPass::BeginConfig {
    std::span<VkClearValue const> clearValues;
    VkExtent2D extent;
    VkSubpassContents subpassContents = VK_SUBPASS_CONTENTS_INLINE;
    VkOffset2D offset{0, 0};
};

}

//State

namespace cth::vk {
struct RenderPass::State {
    vk::not_null<VkRenderPass> vkRenderPass;
};
}

namespace cth::vk {

inline void RenderPass::debug_check(RenderPass const* render_pass) {
    CTH_CRITICAL(render_pass == nullptr, "render pass must not be invalid (nullptr)") {}
    debug_check_handle(render_pass->get());
}
inline void RenderPass::debug_check_handle(VkRenderPass vk_render_pass) {
    CTH_CRITICAL(vk_render_pass == VK_NULL_HANDLE, "vk_render_pass must not be invalid (VK_NULL_HANDLE)") {}
}

}
