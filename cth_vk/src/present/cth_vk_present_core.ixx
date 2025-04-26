module;


export module cth.vk.present.core;

import cth.vk.base.core;
import cth.vk.present.sync.config;
import cth.vk.present.surface;
import cth.vk.present.os_window;
import cth.vk.render.rec.pass;
import cth.vk.submit.queue;
import cth.vk.render.rec.cmd.buffer.base;
import cth.vk.present.basic_swapchain;
import cth.vk.util.types;


namespace cth::vk {
class PresentCore {
public:
    /**
     * @brief describes the state of @ref GraphicsCore
     */
    struct State;


    /**
     * @param core must be created
     */
    explicit PresentCore(Core const& core);

    /**
     * @brief wraps the state
     * @note calls @ref GraphicsCore(Core const&)
     * @note calls @ref wrap()
     */
    PresentCore(Core const& core, State state);


    /**
     * @brief constructs and creates
     * @param present_queue must be valid
     * @note calls @ref create()
     * @note calls @ref GraphicsCore(Core const&)
     */
    PresentCore(Core const& core, std::string_view window_name, VkExtent2D extent, Queue const& present_queue);

    /**
     * @note calls @ref optDestroy()
     */
    ~PresentCore();


    /**
     * @brief constructs osWindow, surface and swapchain
     * @note calls @ref optDestroy()
     */
    void create(std::string_view window_name, VkExtent2D extent, Queue const& present_queue);

    /**
     * @brief wraps the state
     * @note calls @ref optDestroy()
     */
    void wrap(State state);

    /**
     * @brief destroys and resets
     * @attention @ref created() required
     * @note calls @ref reset()
     */
    void destroy();

    /**
     * @brief releases ownership and resets
     * @note does not destroy/delete
     * @note calls @ref reset()
     */
    [[nodiscard]] State release();

    /**
     * brief waits until no longer minimized
     * @note may block
     * @note calls @ref OSWindow::extent
     * @note calls @ref OSWindow::waitEvents
    */
    void minimized() const;

    /**
     * @brief acquires frame from swapchain
     * @note calls @ref BasicSwapchain::acquireNextImage()
     */
    void acquireFrame() const;
    /**
     * @brief skips the acquire
     * @note calls @ref BasicSwapchain::skipAcquire()
     */
    void skipAcquire() const;

    void beginWindowPass(PrimaryCmdBuffer const* render_cmd_buffer) const;
    void endWindowPass(PrimaryCmdBuffer const* render_cmd_buffer) const;

    /**
     * @brief presents the image
     * @note calls @ref BasicSwapchain::present()
     * @note may call @ref BasicSwapchain::resize()
     * @note may call @ref minimized()
     */
    void presentFrame() const;
    void skipPresent() const;

    /**
     * @brief destroys if @ref created()
     */
    void optDestroy() { if(created()) destroy(); }

private:
    void reset();

    cth::not_null<Core const*> _core;
    std::unique_ptr<PresentSyncConfig> _syncConfig;
    std::unique_ptr<OSWindow> _osWindow;
    std::unique_ptr<Surface> _surface;
    std::unique_ptr<BasicSwapchain> _swapchain; //TEMP change to Swapchain ptr once implemented

public:
    [[nodiscard]] bool created() const { return _osWindow || _surface || _swapchain; }
    [[nodiscard]] OSWindow const* osWindow() const { return _osWindow.get(); }
    [[nodiscard]] Surface const* surface() const { return _surface.get(); }
    [[nodiscard]] PresentSyncConfig const* syncConfig() const { return _syncConfig.get(); }
    [[nodiscard]] BasicSwapchain const* swapchain() const { return _swapchain.get(); }
    [[nodiscard]] RenderPass const* swapchainRenderPass() const;
    [[nodiscard]] VkSampleCountFlagBits msaaSamples() const;

    [[nodiscard]] dclauto imageAvailableWaitStages() const { return _syncConfig->imageAvailableWaitStages(); }
    [[nodiscard]] dclauto renderFinishedSemaphores() const { return _syncConfig->renderFinishedSemaphores(); }
    [[nodiscard]] dclauto renderPulse() const { return _syncConfig->pulse(); }
    [[nodiscard]] dclauto pulseVal() const { return _syncConfig->pulseVal(); }

    PresentCore(PresentCore const& other) = delete;
    PresentCore& operator=(PresentCore const& other) = delete;
    PresentCore(PresentCore&& other) noexcept = default;
    PresentCore& operator=(PresentCore&& other) noexcept = default;

    static void debug_check(PresentCore const& graphics_core);
};
}


//State

namespace cth::vk {
struct PresentCore::State {
    unique_not_null<OSWindow> osWindow;
    unique_not_null<Surface> surface;
    unique_not_null<PresentSyncConfig> syncConfig;
    unique_not_null<BasicSwapchain> swapchain;

private:
    static void debug_check(State const& state);

    friend PresentCore;
};
}


//debug check

namespace cth::vk {
inline void PresentCore::debug_check(PresentCore const& graphics_core) {
    CTH_CRITICAL(!graphics_core.created(), "graphics core must be created") {}
}
}
