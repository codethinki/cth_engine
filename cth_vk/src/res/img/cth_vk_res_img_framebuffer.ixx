module;
#include "lib/volk.hpp"
#include <cth/io/io_log.hpp>
export module cth.vk.res.img.framebuffer;

import cth.vk.res.img.view;
import cth.vk.render.rec.attachment_collection;
import cth.vk.base.core;
import cth.vk.base.device_table;
import cth.vk.util.types;
import cth.vk.constants;

import cth.io.log;
import cth.ptr.move;

export namespace cth::vk {

class Framebuffer {
public:
    static constexpr auto DEFAULT_LAYERS = 1;

    struct State;

    /**
     * @brief base constructor
     */
    Framebuffer(Core const& core, vk::not_null<VkRenderPass> render_pass,
        std::span<ImageView const* const> attachments, uint32_t layers = DEFAULT_LAYERS);

    /**
     * @brief constructs and wraps
     * @param state passed to @ref wrap()
     * @note calls @ref Framebuffer(Core const&, RenderPass const&, std::span<ImageView const* const>, uint32_t)
     */
    Framebuffer(Core const& core, vk::not_null<VkRenderPass> render_pass,
        std::span<ImageView const* const> attachments, State const& state, uint32_t layers = DEFAULT_LAYERS);

    /**
     * @brief constructs and creates
     * @param extent passed to @ref create()
     * @note calls @ref Framebuffer(Core const&, RenderPass const&, std::span<ImageView const* const>, uint32_t)
     */
    Framebuffer(Core const& core, vk::not_null<VkRenderPass> const& render_pass,
        std::span<ImageView const* const> attachments, VkExtent2D extent, uint32_t layers = DEFAULT_LAYERS);

    ~Framebuffer();

    /**
     * @brief wraps @ref State
     * @note calls @ref optDestroy()
     */
    void wrap(State const& state);

    /**
     * @brief creates the framebuffer
     * @param extent framebuffer extent
     * @note calls @ref optDestroy()
     */
    void create(VkExtent2D extent);

    /**
     * @brief destroys the framebuffer
     * @attention requires @ref created()
     * @note if @ref Core::destructionQueue() -> pushes to queue
     * @note calls @ref destroy(vk::not_null<VkDevice>, VkFramebuffer)
     */
    void destroy();
    /**
     * @brief if @ref created() calls @ref destroy()
     */
    void optDestroy() { if(created()) destroy(); }

    /**
     * @brief releases ownership and resets
     * @attention requires @ref created()
     */
    State release();

    static void destroy(DeviceTable table, VkFramebuffer vk_framebuffer);

private:
    void reset();

    cth::not_null<Core const*> _core;
    vk::not_null<VkRenderPass> _renderPass;
    std::vector<ImageView const*> _attachments;
    uint32_t _layers;

    cth::move_ptr<VkFramebuffer_T> _handle = VK_NULL_HANDLE;
    VkExtent2D _extent{};

public:
    [[nodiscard]] VkFramebuffer get() const { return _handle.get(); }
    [[nodiscard]] bool created() const { return _handle != VK_NULL_HANDLE; }

    Framebuffer(Framebuffer const& other) = delete;
    Framebuffer(Framebuffer&& other) noexcept = default;
    Framebuffer& operator=(Framebuffer const& other) = delete;
    Framebuffer& operator=(Framebuffer&& other) noexcept = default;

    static void debug_check(Framebuffer const& framebuffer);
    static void debug_check_handle(vk::not_null<VkFramebuffer> vk_framebuffer);

};
}

//State

export namespace cth::vk {
struct Framebuffer::State {
    vk::not_null<VkFramebuffer> vkFramebuffer;
    VkExtent2D extent;
};
}

//debug checks

export namespace cth::vk {
inline void Framebuffer::debug_check(Framebuffer const& framebuffer) {
    CTH_CRITICAL(!framebuffer.created(), "framebuffer must be created") {}
    debug_check_handle(framebuffer.get());
}
inline void Framebuffer::debug_check_handle([[maybe_unused]] vk::not_null<VkFramebuffer> vk_framebuffer) {}

}
