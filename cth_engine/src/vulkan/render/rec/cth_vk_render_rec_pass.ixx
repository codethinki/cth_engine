module;
#include <cth/io/io_log.hpp>


export module cth.vk.render.rec.pass;

import cth.vk.base.device_table;
import cth.vk.constants;
import cth.vk.util.types;
import cth.vk.render.rec.cmd.buffer;
import cth.vk.render.rec.attachment_collection;
import cth.vk.render.rec.subpass;
import cth.vk.res.img.framebuffer;
import cth.vk.base.core;


import cth.ptr;
import cth.io.log;

import std;

export namespace cth::vk {


class RenderPass {
public:
    struct BeginConfig;
    struct State;

    /**
     * @brief base constructor
     */
    RenderPass(Core const& core, std::span<Subpass const* const> subpasses,
        std::span<VkSubpassDependency const> dependencies, std::span<BeginConfig const> begin_configs);

    /**
     * @brief constructs and calls @ref wrap(State const&)
     * @note calls @ref RenderPass(Core const&, std::span<Subpass const* const>, std::span<VkSubpassDependency const>, std::span<BeginConfig const>)
     */
    RenderPass(Core const& core, std::span<Subpass const* const> subpasses,
        std::span<VkSubpassDependency const> dependencies, std::span<BeginConfig const> begin_configs, State const& state);

    /**
     * @brief constructs and creates
     * @note calls @ref RenderPass(Core const&, std::span<Subpass const* const>, std::span<VkSubpassDependency const>, std::span<BeginConfig const>)
     */
    RenderPass(Core const& core, std::span<Subpass const* const> subpasses,
        std::span<VkSubpassDependency const> dependencies, std::span<BeginConfig const> begin_configs,
        create_t);


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

    void begin(cth::vk::not_null<VkCommandBuffer> cmd_buffer, uint32_t config_index, Framebuffer const& framebuffer);
    void end(cth::vk::not_null<VkCommandBuffer> cmd_buffer);

    static void destroy(DeviceTable table, VkRenderPass vk_render_pass);

private:
    void reset();

    void initAttachments();

    cth::not_null<Core const*> _core;

    move_ptr<VkRenderPass_T> _handle;

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

export namespace cth::vk {
struct RenderPass::BeginConfig {
    std::span<VkClearValue const> clearValues;
    VkExtent2D extent;
    VkSubpassContents subpassContents = VK_SUBPASS_CONTENTS_INLINE;
    VkOffset2D offset{0, 0};
};

}

//State

export namespace cth::vk {
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
