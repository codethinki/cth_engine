#pragma once
#include "../../../src/utility/graphics_core_config.hpp"

#include "jolly/render/RenderPulse.hpp"

#include "jolly/utility/GraphicsSyncConfig.hpp"
#include "jvk/base/core.hpp"


namespace jvk {
class AttachmentCollection;
class GraphicsSyncConfig;
class Surface;
struct Cycle;
class RenderPass;
}

namespace jly {
class OSWindow;


class GraphicsCore {
public:
    /**
     * @brief describes the state of @ref GraphicsCore
     */
    struct State;

    using Config = GraphicsCoreConfig;


    /**
     * @param core must be created
     */
    explicit GraphicsCore(jvk::Core const& core, Config config);

    /**
     * @brief wraps the state
     * @note calls @ref GraphicsCore(Core const&, Config)
     * @note calls @ref wrap()
     */
    GraphicsCore(jvk::Core const& core, Config const& config, State state);


    /**
     * @brief constructs and creates
     * @param present_queue must be valid
     * @note calls @ref create()
     * @note calls @ref GraphicsCore(Core const&)
     */
    GraphicsCore(jvk::Core const& core, Config const& config, std::string_view window_name, VkExtent2D extent,
        jvk::Queue const& present_queue);

    /**
     * @note calls @ref optDestroy()
     */
    ~GraphicsCore();


    /**
     * @brief constructs osWindow, surface and swapchain
     * @note calls @ref optDestroy()
     */
    void create(std::string_view window_name, VkExtent2D extent, jvk::Queue const& present_queue);

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
     * @note calls @ref Swapchain::acquireNextImage()
     */
    void acquireFrame() const;
    /**
     * @brief skips the acquire
     * @note calls @ref Swapchain::skipAcquire()
     */
    void skipAcquire() const;

    /**
     * @brief presents the image
     * @note calls @ref Swapchain::present()
     * @note may call @ref Swapchain::resize()
     * @note may call @ref minimized()
     * @return true if swapchain was resized
     */
    [[nodiscard]] bool presentFrame() const;
    void skipPresent() const;

    /**
     * @brief destroys if @ref created()
     */
    void optDestroy() { if(created()) destroy(); }

private:
    void reset();

    cth::not_null<jvk::Core const*> _core;
    Config _config;

    std::unique_ptr<GraphicsSyncConfig> _syncConfig;
    std::unique_ptr<OSWindow> _osWindow;
    std::unique_ptr<jvk::Surface> _surface;
    std::unique_ptr<jvk::Swapchain> _swapchain; //TEMP change to Swapchain ptr once implemented

public:
    [[nodiscard]] bool created() const { return _osWindow || _surface || _swapchain; }
    [[nodiscard]] OSWindow const* osWindow() const { return _osWindow.get(); }
    [[nodiscard]] jvk::Surface const* surface() const { return _surface.get(); }
    [[nodiscard]] GraphicsSyncConfig const* syncConfig() const { return _syncConfig.get(); }
    [[nodiscard]] jvk::Swapchain const* swapchain() const { return _swapchain.get(); }
    [[nodiscard]] VkSampleCountFlagBits msaaSamples() const;

    [[nodiscard]] jvk::AttachmentCollection const* swapchainResolveAttachments() const;
    [[nodiscard]] VkFormat swapchainImageFormat() const;
    [[nodiscard]] VkExtent2D swapchainExtent() const;
    [[nodiscard]] size_t swapchainSize() const;
    [[nodiscard]] size_t swapchainImageIndex(RenderPulse const& pulse) const;

    [[nodiscard]] dclauto imageAvailableWaitStages() const { return _syncConfig->imageAvailableWaitStages(); }
    [[nodiscard]] dclauto renderFinishedSemaphores() const { return _syncConfig->renderFinishedSemaphores(); }
    [[nodiscard]] dclauto renderPulse() const { return _syncConfig->pulse(); }
    [[nodiscard]] dclauto pulseVal() const { return _syncConfig->pulseVal(); }

    GraphicsCore(GraphicsCore const& other) = delete;
    GraphicsCore& operator=(GraphicsCore const& other) = delete;
    GraphicsCore(GraphicsCore&& other) noexcept = default;
    GraphicsCore& operator=(GraphicsCore&& other) noexcept = default;

    static void debug_check(GraphicsCore const& graphics_core);
};
}


//State

namespace jly {
struct GraphicsCore::State {
    cth::unique_not_null<OSWindow> osWindow;
    cth::unique_not_null<jvk::Surface> surface;
    cth::unique_not_null<GraphicsSyncConfig> syncConfig;
    cth::unique_not_null<jvk::Swapchain> swapchain;

private:
    static void debug_check(State const& state);

    friend GraphicsCore;
};
}


//debug check

namespace jly {
inline void GraphicsCore::debug_check(GraphicsCore const& graphics_core) {
    CTH_CRITICAL(!graphics_core.created(), "graphics core must be created") {}
}
}
