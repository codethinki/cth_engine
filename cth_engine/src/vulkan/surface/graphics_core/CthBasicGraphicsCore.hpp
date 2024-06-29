#pragma once

#include <span>
#include <vector>
#include <xstring>

#include "CthGraphicsSyncConfig.hpp"
#include "vulkan/base/CthQueue.hpp"
#include "vulkan/utility/CthConstants.hpp"



namespace cth {
class BasicCore;
class DeletionQueue;
class Surface;
class OSWindow;
class BasicSwapchain;
class PrimaryCmdBuffer;

class BasicGraphicsCore {
public:
    BasicGraphicsCore(const BasicCore* core, OSWindow* os_window, Surface* surface, BasicSwapchain* swapchain);
    BasicGraphicsCore() = default;
    virtual ~BasicGraphicsCore() = default;

    /**
     * \brief constructs osWindow, surface and swapchain
     * \note does not destroy/delete
     */
    virtual void create(std::string_view window_name, uint32_t width, uint32_t height, const Queue* present_queue,
        const GraphicsSyncConfig& sync_config);


    /**
     * \brief destroys & deletes osWindow, surface and swapchain
     * \param deletion_queue optional
     * \note implicitly calls reset();
     */
    virtual void destroy(DeletionQueue* deletion_queue = nullptr);

    /**
     * \brief sets osWindow, surface and swapchain to nullptr
     * \note does not destroy/delete
     */
    void reset();




    void acquireFrame() const;

    void beginWindowPass(const PrimaryCmdBuffer* render_cmd_buffer) const;
    void endWindowPass(const PrimaryCmdBuffer* render_cmd_buffer) const;

    void presentFrame(DeletionQueue* deletion_queue) const; //TEMP

private:
    const BasicCore* _core;

    OSWindow* _osWindow;
    Surface* _surface;
    BasicSwapchain* _swapchain;

public:
    [[nodiscard]] const OSWindow* osWindow() const { return _osWindow; }
    [[nodiscard]] const Surface* surface() const { return _surface; }
    [[nodiscard]] const BasicSwapchain* swapchain() const { return _swapchain; }


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
