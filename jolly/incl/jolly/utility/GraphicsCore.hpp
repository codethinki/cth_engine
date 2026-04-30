#pragma once
#include "graphics_core_config.hpp"

#include "jolly/utility/GraphicsSyncConfig.hpp"

#include <cth/macro.hpp>


namespace jvk {
class AttachmentCollection;
class Surface;
class Swapchain;
}

namespace jly {
class Queue;
class OSWindow;
}

namespace jly {


class GraphicsCore {
public:
    /**
     * @brief describes the state of @ref GraphicsCore
     */
    struct State;

    using Config = GraphicsCoreConfig;


    explicit GraphicsCore(jly::Core const&, Config);

    /**
     * @brief wraps the state
     * @note calls @ref GraphicsCore(Core const&, Config)
     * @note calls @ref wrap()
     */
    GraphicsCore(jly::Core const&, Config const&, State);


    /**
     * @brief constructs and creates
     * @param present_queue must be valid
     * @note calls @ref create()
     * @note calls @ref GraphicsCore(Core const&, Config)
     */
    GraphicsCore(
        jly::Core const&,
        Config const&,
        std::string_view window_name,
        glm::uvec2 extent,
        Queue const& present_queue
    );

    /**
     * @note calls @ref optDestroy()
     */
    ~GraphicsCore();


    /**
     * @brief constructs osWindow, surface and swapchain
     * @note calls @ref optDestroy()
     */
    void create(std::string_view window_name, glm::uvec2 extent, Queue const& present_queue);

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
    void acquireFrame();
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
     * @return true if core should be resized
     */
    [[nodiscard]] bool presentFrame();
    void skipPresent() const;

    /**
     * @brief destroys if @ref created()
     */
    void optDestroy() { if(created()) destroy(); }


    /**
     * @brief resizes the window and swapchain
     */
    void resize();

private:
    void reset();


    bool _resize = false;

    not_null<jly::Core const*> _core;
    Config _config;

    std::unique_ptr<GraphicsSyncConfig> _syncConfig;
    std::unique_ptr<OSWindow> _osWindow;
    std::unique_ptr<jvk::Surface> _surface;
    std::unique_ptr<jvk::Swapchain> _swapchain; //TEMP change to Swapchain ptr once implemented

public:
    [[nodiscard]] auto framesInFlight() const { return _config.framesInFlight; }

    [[nodiscard]] bool created() const { return _osWindow || _surface || _swapchain; }
    [[nodiscard]] auto const* osWindow() const { return _osWindow.get(); }
    [[nodiscard]] auto const* surface() const { return _surface.get(); }
    [[nodiscard]] auto const* syncConfig() const { return _syncConfig.get(); }
    [[nodiscard]] auto const* swapchain() const { return _swapchain.get(); }

    [[nodiscard]] bool shouldResize() const { return _resize; }

    [[nodiscard]] auto const& vk_core() const { return *_core; }

    [[nodiscard]] jvk::AttachmentCollection const* swapchainResolveAttachments() const;
    [[nodiscard]] VkFormat swapchainImageFormat() const;
    [[nodiscard]] glm::uvec2 swapchainExtent() const;
    [[nodiscard]] size_t swapchainSize() const;
    [[nodiscard]] size_t swapchainImageIndex() const;

    [[nodiscard]] declauto renderPulse() const { return _syncConfig->pulse(); }
    [[nodiscard]] declauto pulseVal() const { return _syncConfig->pulseVal(); }

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
