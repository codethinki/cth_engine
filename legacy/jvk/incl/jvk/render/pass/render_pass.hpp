#pragma once
#include "render_pass_begin_config.hpp"

#include "jvk/base/device_table.hpp"
#include "jvk/utility/constants.hpp"
#include "jvk/utility/types.hpp"

#include <cth/ptr/move_ptr.hpp>



namespace jvk {
class PrimaryCmdBuffer;
class AttachmentCollection;
class Subpass;
class Core;
class Framebuffer;

struct RenderPassConfig;

class RenderPass {
public:
    using Config = RenderPassConfig;
    using BeginConfig = RenderPassBeginConfig;
    struct State;

    /**
     * @brief base constructor
     */
    RenderPass(Core const& core, Config const& config);

    /**
     * @brief constructs and calls @ref wrap(State const&)
     * @note calls @ref RenderPass(Core const&, Config const&)
     */
    RenderPass(Core const& core, Config const& config, State const& state);

    /**
     * @brief constructs and creates
     * @note calls @ref RenderPass(Core const&, Config const&)
     */
    RenderPass(Core const& core, Config const& config, create_t);


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
     * @throws jvk::result_exception result of vkCreateRenderPass
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

    void resize(VkExtent2D extent, VkOffset2D offset = {});
    void resize(VkRect2D area);
    void recolor(std::span<VkClearValue const> clear_values);
    void relocateSubpassContents(VkSubpassContents contents);

    void begin(PrimaryCmdBuffer const& cmd_buffer, Framebuffer const& framebuffer);
    void end(PrimaryCmdBuffer const& cmd_buffer);

    static void destroy(DeviceTable table, VkRenderPass vk_render_pass);

private:
    void reset();
    void setHandle(VkRenderPass handle);


    std::vector<VkAttachmentDescription> attachmentDescriptions() const;
    void initAttachments();


    not_null<Core const*> _core;

    move_ptr<VkRenderPass_T> _handle;

    std::vector<Subpass const*> _subpasses;
    std::vector<AttachmentCollection const*> _attachments;
    std::vector<VkSubpassDependency> _dependencies;
    VkRenderPassBeginInfo _beginInfo{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, nullptr};
    VkSubpassContents _subpassContents;
    std::vector<VkClearValue> _clearValue;

public:
    [[nodiscard]] VkRenderPass get() const { return _handle.get(); }
    [[nodiscard]] bool created() const { return _handle.get() != VK_NULL_HANDLE; }

    RenderPass(RenderPass const& other) = delete;
    RenderPass(RenderPass&& other) noexcept = default;
    RenderPass& operator=(RenderPass const& other) = delete;
    RenderPass& operator=(RenderPass&& other) noexcept = default;


    static void debug_check(RenderPass const& render_pass);
    static void debug_check_handle(VkRenderPass vk_render_pass);
};


}


//State

namespace jvk {
struct RenderPass::State {
    jvk::vk_not_null<VkRenderPass> vkRenderPass;
};
}

namespace jvk {

inline void RenderPass::debug_check(RenderPass const& render_pass) {


    debug_check_handle(render_pass.get());
}

inline void RenderPass::debug_check_handle(VkRenderPass vk_render_pass) {
    CTH_CRITICAL(vk_render_pass == VK_NULL_HANDLE, "vk_render_pass must not be invalid (VK_NULL_HANDLE)") {}
}

}