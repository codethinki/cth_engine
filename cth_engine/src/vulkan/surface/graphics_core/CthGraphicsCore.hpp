#pragma once
#include "CthGraphicsSyncConfig.hpp"

#include "vulkan/base/CthCore.hpp"
#include "vulkan/base/CthQueue.hpp"
#include "vulkan/utility/cth_constants.hpp"
#include "vulkan/utility/cth_debug_macros.hpp"



namespace cth::vk {
class Surface;
class OSWindow;
struct Cycle;
class RenderPass;

class GraphicsCore {
public:
    /**
     * @brief describes the state of @ref GraphicsCore
     */
    struct State;


    /**
     * @param core must be created
     */
    explicit GraphicsCore(not_null<BasicCore const*> core);

    /**
     * @brief wraps the state
     * @note calls @ref GraphicsCore(not_null<BasicCore const*>)
     * @note calls @ref wrap()
     */
    GraphicsCore(not_null<BasicCore const*> core, State state);


    /**
     * @brief constructs and creates
     * @param present_queue must be valid
     * @param sync_config must be valid
     * @note calls @ref create()
     * @note calls @ref GraphicsCore(not_null<BasicCore const*>)
     */
    GraphicsCore(not_null<BasicCore const*> core, std::string_view window_name, VkExtent2D extent, not_null<Queue const*> present_queue,
        not_null<GraphicsSyncConfig const*> sync_config);

    /**
     * @note calls @ref optDestroy()
     */
    ~GraphicsCore();


    /**
     * @brief constructs osWindow, surface and swapchain
     * @note calls @ref optDestroy()
     */
    void create(std::string_view window_name, VkExtent2D extent, not_null<Queue const*> present_queue,
        not_null<GraphicsSyncConfig const*> sync_config);

    /**
     * @brief wraps the state
     * @note calls @ref optDestroy()
     */
    void wrap(State state);

    /**
     * @brief destroys and resets
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
    void acquireFrame(Cycle const& cycle) const;
    /**
     * @brief skips the acquire
     * @note calls @ref BasicSwapchain::skipAcquire()
     */
    void skipAcquire(Cycle const& cycle) const;

    void beginWindowPass(Cycle const& cycle, PrimaryCmdBuffer const* render_cmd_buffer) const;
    void endWindowPass(PrimaryCmdBuffer const* render_cmd_buffer) const;

    /**
     * @brief presents the image
     * @note calls @ref BasicSwapchain::present()
     * @note may call @ref BasicSwapchain::resize()
     * @note may call @ref minimized()
     */
    void presentFrame(Cycle const& cycle) const;
    void skipPresent(Cycle const& cycle) const;

    /**
     * @brief destroys if @ref created()
     */
    void optDestroy() { if(created()) destroy(); }

private:
    void reset();

    not_null<BasicCore const*> _core;
    std::unique_ptr<OSWindow> _osWindow = nullptr;
    std::unique_ptr<Surface> _surface = nullptr;
    std::unique_ptr<BasicSwapchain> _swapchain = nullptr; //TODO change to Swapchain ptr once implemented

public:
    [[nodiscard]] bool created() const { return _osWindow || _surface || _swapchain; }
    [[nodiscard]] OSWindow const* osWindow() const { return _osWindow.get(); }
    [[nodiscard]] Surface const* surface() const { return _surface.get(); }
    [[nodiscard]] BasicSwapchain const* swapchain() const { return _swapchain.get(); }
    [[nodiscard]] RenderPass const* swapchainRenderPass() const;
    [[nodiscard]] VkSampleCountFlagBits msaaSamples() const;

    GraphicsCore(GraphicsCore const& other) = delete;
    GraphicsCore(GraphicsCore&& other) noexcept = default;
    GraphicsCore& operator=(GraphicsCore const& other) = delete;
    GraphicsCore& operator=(GraphicsCore&& other) noexcept = default;
#ifdef CONSTANT_DEBUG_MODE
    static void debug_check(not_null<GraphicsCore const*> graphics_core);
    static void debug_check_state(State const& state);

#define DEBUG_CHECK_GRAPHICS_CORE_STATE(state)
#define DEBUG_CHECK_GRAPHICS_CORE(graphics_core_ptr)

#else
#define DEBUG_CHECK_GRAPHICS_CORE_STATE(state) ((void)0)
#define DEBUG_CHECK_GRAPHICS_CORE(graphics_core_ptr) ((void)0)

#endif
};
}

//State

namespace cth::vk {
struct GraphicsCore::State {
    unique_not_null<OSWindow> osWindow;
    unique_not_null<Surface> surface;
    unique_not_null<BasicSwapchain> swapchain;
};
}
