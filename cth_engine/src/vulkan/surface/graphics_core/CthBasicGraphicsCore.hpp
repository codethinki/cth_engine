#pragma once

#include <span>
#include <vector>
#include <xstring>

#include "CthGraphicsSyncConfig.hpp"
#include "../swapchain/CthBasicSwapchain.hpp"

#include "vulkan/base/CthCore.hpp"
#include "vulkan/base/CthQueue.hpp"
#include "vulkan/utility/cth_constants.hpp"
#include "vulkan/utility/cth_debug_macros.hpp"



namespace cth::vk {
class BasicCore;
class DestructionQueue;
class Surface;
class OSWindow;
class BasicSwapchain;
class PrimaryCmdBuffer;

class BasicGraphicsCore {
public:
    struct handles {
        OSWindow* osWindow = nullptr;
        Surface* surface = nullptr;
        BasicSwapchain* swapchain = nullptr;
    };


    BasicGraphicsCore(BasicCore const* core, OSWindow* os_window, Surface* surface, BasicSwapchain* swapchain);
    explicit BasicGraphicsCore(BasicCore const* core) : _core(core) {
        DEBUG_CHECK_CORE(core);
    }
    virtual ~BasicGraphicsCore() CTH_DEBUG_IMPL;


    virtual void wrap(OSWindow* os_window, Surface* surface, BasicSwapchain* swapchain);

    /**
     * @brief constructs osWindow, surface and swapchain
     * @note does not destroy/delete
     */
    virtual void create(std::string_view const window_name, VkExtent2D const extent, Queue const* present_queue,
        BasicGraphicsSyncConfig const* sync_config, DestructionQueue* destruction_queue = nullptr);


    /**
     * @brief destroys & deletes osWindow, surface and swapchain
     * @param destruction_queue optional
     * @note release() gets implicitly called
     */
    virtual void destroy(DestructionQueue* destruction_queue = nullptr);

    /**
     * @brief sets osWindow, surface and swapchain to nullptr
     * @note does not destroy/delete
     */
    [[nodiscard]] handles release();

    /**
     * brief waits until no longer minimized
     * @return new window extent
    */
    void minimized() const;

    void acquireFrame(Cycle const& cycle) const;
    void skipAcquire(Cycle const& cycle) const;

    void beginWindowPass(Cycle const& cycle, PrimaryCmdBuffer const* render_cmd_buffer) const;
    void endWindowPass(PrimaryCmdBuffer const* render_cmd_buffer) const;

    void presentFrame(Cycle const& cycle) const;
    void skipPresent(Cycle const& cycle) const;

protected:
    BasicCore const* _core;

private:
    move_ptr<OSWindow> _osWindow = nullptr;
    move_ptr<Surface> _surface = nullptr;
    move_ptr<BasicSwapchain> _swapchain = nullptr; //TODO change to Swapchain ptr once implemented

public:
    [[nodiscard]] OSWindow const* osWindow() const { return _osWindow.get(); }
    [[nodiscard]] Surface const* surface() const { return _surface.get(); }
    [[nodiscard]] BasicSwapchain const* swapchain() const { return _swapchain.get(); }
    [[nodiscard]] VkRenderPass swapchainRenderPass() const { return _swapchain->renderPass(); }
    [[nodiscard]] VkSampleCountFlagBits msaaSamples() const { return _swapchain->msaaSamples(); }


#ifdef CONSTANT_DEBUG_MODE
    static void debug_check_not_null(BasicGraphicsCore const* graphics_core);
    static void debug_check_leak(BasicGraphicsCore const* graphics_core);

#define DEBUG_CHECK_GRAPHICS_CORE_LEAK(graphics_core) BasicGraphicsCore::debug_check_leak(graphics_core)
#define DEBUG_CHECK_GRAPHICS_CORE_NOT_NULL(graphics_core) BasicGraphicsCore::debug_check_not_null(graphics_core)
#else
#define DEBUG_CHECK_GRAPHCIS_CORE_NOT_NULL(graphics_core) ((void)0)
#define DEBUG_CHECK_GRAPHICS_CORE_LEAK(graphics_core) ((void)0)
#endif


    BasicGraphicsCore(BasicGraphicsCore const& other) = default;
    BasicGraphicsCore(BasicGraphicsCore&& other) noexcept = default;
    BasicGraphicsCore& operator=(BasicGraphicsCore const& other) = default;
    BasicGraphicsCore& operator=(BasicGraphicsCore&& other) noexcept = default;
};
}
