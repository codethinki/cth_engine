#pragma once

#include <span>
#include <vector>
#include <xstring>

#include "CthGraphicsSyncConfig.hpp"
#include "../swapchain/CthBasicSwapchain.hpp"
#include "vulkan/base/CthQueue.hpp"
#include "vulkan/utility/CthConstants.hpp"
#include "vulkan/utility/cth_debug.hpp"



namespace cth {
class BasicCore;
class DeletionQueue;
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


    BasicGraphicsCore(const BasicCore* core, OSWindow* os_window, Surface* surface, BasicSwapchain* swapchain);
    explicit BasicGraphicsCore(const BasicCore* core) : _core(core) {}
    virtual ~BasicGraphicsCore() CTH_DEBUG_IMPL;


    virtual void wrap(OSWindow* os_window, Surface* surface, BasicSwapchain* swapchain);

    /**
     * @brief constructs osWindow, surface and swapchain
     * @note does not destroy/delete
     */
    virtual void create(std::string_view window_name, VkExtent2D extent, const Queue* present_queue,
        const BasicGraphicsSyncConfig& sync_config, DeletionQueue* deletion_queue = nullptr);


    /**
     * @brief destroys & deletes osWindow, surface and swapchain
     * @param deletion_queue optional
     * @note release() gets implicitly called
     */
    virtual void destroy(DeletionQueue* deletion_queue = nullptr);

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

    void acquireFrame() const;

    void beginWindowPass(const PrimaryCmdBuffer* render_cmd_buffer) const;
    void endWindowPass(const PrimaryCmdBuffer* render_cmd_buffer) const;

    void presentFrame(DeletionQueue* deletion_queue) const; //TEMP

private:
    const BasicCore* _core;

    move_ptr<OSWindow> _osWindow = nullptr;
    move_ptr<Surface> _surface = nullptr;
    move_ptr<BasicSwapchain> _swapchain = nullptr; //TODO change to Swapchain ptr once implemented

public:
    [[nodiscard]] const OSWindow* osWindow() const { return _osWindow.get(); }
    [[nodiscard]] const Surface* surface() const { return _surface.get(); }
    [[nodiscard]] const BasicSwapchain* swapchain() const { return _swapchain.get(); }
    [[nodiscard]] VkRenderPass swapchainRenderPass() const { return _swapchain->renderPass(); }
    [[nodiscard]] VkSampleCountFlagBits msaaSamples() const { return _swapchain->msaaSamples(); }


#ifdef CONSTANT_DEBUG_MODE
    static void debug_check_not_null(const BasicGraphicsCore* graphics_core);
    static void debug_check_leak(const BasicGraphicsCore* graphics_core);

#define DEBUG_CHECK_GRAPHICS_CORE_LEAK(graphics_core) BasicGraphicsCore::debug_check_leak(graphics_core)
#define DEBUG_CHECK_GRAPHICS_CORE_NOT_NULL(graphics_core) BasicGraphicsCore::debug_check_not_null(graphics_core)
#else
#define DEBUG_CHECK_GRAPHCIS_CORE_NOT_NULL(graphics_core) ((void)0)
#define DEBUG_CHECK_GRAPHICS_CORE_LEAK(graphics_core) ((void)0)
#endif


    BasicGraphicsCore(const BasicGraphicsCore& other) = default;
    BasicGraphicsCore(BasicGraphicsCore&& other) noexcept = default;
    BasicGraphicsCore& operator=(const BasicGraphicsCore& other) = default;
    BasicGraphicsCore& operator=(BasicGraphicsCore&& other) noexcept = default;
};
}
