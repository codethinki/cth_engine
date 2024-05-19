#pragma once

#include "vulkan/utility/CthConstants.hpp"



namespace cth {
class Surface;
class OSWindow;
class BasicSwapchain;
class PrimaryCmdBuffer;

class BasicWindow {
public:
    BasicWindow(OSWindow* os_window, Surface* surface, BasicSwapchain* swapchain);
    ~BasicWindow();

    void beginWindowPass(const PrimaryCmdBuffer* render_cmd_buffer);
    void endWindowPass(const PrimaryCmdBuffer* render_cmd_buffer);

private:
    OSWindow* _osWindow;
    Surface* _surface;
    BasicSwapchain* _swapchain;

public:
    [[nodiscard]] const OSWindow* osWindow() const { return _osWindow; }
    [[nodiscard]] const Surface* surface() const { return _surface; }
    [[nodiscard]] const BasicSwapchain* swapchain() const { return _swapchain; }
};
}
