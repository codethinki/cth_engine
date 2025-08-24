#pragma once
#include "jvk/utility/constants.hpp"

#include <memory>
#include <string>
#include <vector>

#include <cth/io/log.hpp>
#include <cth/pointer/move_ptr.hpp>

using window_handle_t = std::add_pointer_t<struct GLFWwindow>;

namespace jvk {
class DestructionQueue;
class Surface;
class Instance;
}

namespace jly {
//TODO implement DEBUG_CHECK_OS_WINDOW
//TEMP modernize
class OSWindow {
    using handle_t = window_handle_t;
    using window_t = std::remove_pointer_t<handle_t>;

public:
    OSWindow(jvk::Instance const& instance, jvk::DestructionQueue* destruction_queue, std::string_view name,
        VkExtent2D extent);
    ~OSWindow();

    void destroy(jvk::DestructionQueue* destruction_queue = nullptr);

    void resetWindowResized() { _framebufferResized = false; }
    void waitEvents();


    static std::vector<std::string> getGLFWInstanceExtensions();
    static void init();
    static void terminate();

    static void destroy(handle_t glfw_window);

private:
    void initWindow();
    void setCallbacks();
    void createSurface(jvk::Instance const& instance);


    void keyCallback(int key, int scan_code, int action, int mods);
    void mouseCallback(int button, int action);
    void scrollCallback(double x_offset, double y_offset);
    void focusCallback(int focused);
    void framebufferResizeCallback(int new_width, int new_height);


    jvk::Instance const* _instance = nullptr;
    jvk::DestructionQueue* _destructionQueue;

    bool _focus = true;
    bool _framebufferResized = false;

    std::string _windowName;
    int _width, _height;

    cth::move_ptr<window_t> _handle;
    cth::move_ptr<VkSurfaceKHR_T> _surface;

    static OSWindow* window_ptr(GLFWwindow* glfw_window);

    static void setGLFWWindowHints();

    static void staticKeyCallback(handle_t glfw_window, int key, int scan_code, int action, int mods);
    static void staticMouseCallback(handle_t glfw_window, int button, int action, int mods);
    static void staticScrollCallback(handle_t glfw_window, double x_offset, double y_offset);
    static void staticFramebufferResizeCallback(handle_t glfw_window, int width, int height);
    static void staticFocusCallback(handle_t glfw_window, int focused);

public:
    [[nodiscard]] bool shouldClose() const;

    [[nodiscard]] VkExtent2D extent() const {
        return {static_cast<uint32_t>(_width), static_cast<uint32_t>(_height)};
    }

    [[nodiscard]] bool windowResized() const { return _framebufferResized; }
    [[nodiscard]] bool focused() const { return _focus; }

    [[nodiscard]] VkSurfaceKHR releaseSurface() {
        auto const handle = _surface.get();
        _surface = VK_NULL_HANDLE;
        return handle;
    }



    OSWindow(OSWindow const& other) = delete;
    OSWindow& operator=(OSWindow const& other) = delete;
    OSWindow(OSWindow&& other) = default;
    OSWindow& operator=(OSWindow&& other) = default; // copy/move operations

    static void debug_check_not_null(OSWindow const* os_window);
    static void debug_check(OSWindow const* os_window);

};
}

namespace jly {

inline void OSWindow::debug_check_not_null(OSWindow const* os_window) {
    CTH_CRITICAL(os_window == nullptr, "os_window must not be nullptr") {}
}

inline void OSWindow::debug_check(OSWindow const* os_window) {
    OSWindow::debug_check_not_null(os_window);

    CTH_CRITICAL(os_window->_handle == nullptr, "os_window must be initialized") {}
}
}
