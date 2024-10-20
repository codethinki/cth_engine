#pragma once
#include "vulkan/utility/cth_constants.hpp"
#include "vulkan/utility/cth_vk_types.hpp"

#include <gsl/pointers>
#include <volk.h>

namespace cth::vk {
class Core;
class RenderPass;
class AttachmentCollection;
class ImageView;


class Framebuffer {
public:
    static constexpr auto DEFAULT_LAYERS = 1;

    struct State;

    /**
     * @brief base constructor
     * @param render_pass requires RenderPass::created()
     */
    Framebuffer(cth::not_null<Core const*> core, cth::not_null<RenderPass const*> render_pass,
        std::span<ImageView const* const> attachments, uint32_t layers = DEFAULT_LAYERS);

    /**
     * @brief constructs and wraps
     * @param state passed to @ref wrap()
     * @note calls @ref Framebuffer(cth::not_null<Core const*>, cth::not_null<RenderPass const*>, std::span<ImageView const* const>, uint32_t)
     */
    Framebuffer(cth::not_null<Core const*> core, cth::not_null<RenderPass const*> render_pass,
        std::span<ImageView const* const> attachments, State const& state, uint32_t layers = DEFAULT_LAYERS);

    /**
     * @brief constructs and creates
     * @param extent passed to @ref create()
     * @note calls @ref Framebuffer(cth::not_null<Core const*>, cth::not_null<RenderPass const*>, std::span<ImageView const* const>, uint32_t)
     */
    Framebuffer(cth::not_null<Core const*> core, cth::not_null<RenderPass const*> render_pass,
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

    static void destroy(vk::not_null<VkDevice> vk_device, VkFramebuffer vk_framebuffer);

private:
    void reset();

    cth::not_null<Core const*> _core;
    cth::not_null<RenderPass const*> _renderPass;
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

    static void debug_check(cth::not_null<Framebuffer const*> framebuffer);
    static void debug_check_handle(vk::not_null<VkFramebuffer> vk_framebuffer);

};
}

//State

namespace cth::vk {
struct Framebuffer::State {
    vk::not_null<VkFramebuffer> vkFramebuffer;
    VkExtent2D extent;
};
}

//debug checks

namespace cth::vk {
inline void Framebuffer::debug_check(cth::not_null<Framebuffer const*> framebuffer) {
    CTH_ERR(!framebuffer->created(), "framebuffer must be created") throw details->exception();
    debug_check_handle(framebuffer->get());
}
inline void Framebuffer::debug_check_handle([[maybe_unused]] vk::not_null<VkFramebuffer> vk_framebuffer) {}

}
