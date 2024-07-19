#pragma once
#include "vulkan/utility/CthConstants.hpp"


#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <memory>
#include <string>
#include <vector>


namespace cth::vk {
class BasicInstance;
class Surface;
//TODO implement DEBUG_CHECK_OS_WINDOW

class OSWindow {
public:
    OSWindow(std::string_view name, uint32_t width, uint32_t height, const BasicInstance* instance);
    ~OSWindow();

    void resetWindowResized() { _framebufferResized = false; }
    void waitEvents() { glfwWaitEvents(); }

    static std::vector<std::string> getGLFWInstanceExtensions();
    static void init();
    static void terminate();

    static VkSurfaceKHR tempSurface(const BasicInstance* instance);

private:
    void initWindow();
    void setCallbacks();
    void createSurface(const BasicInstance* instance);


    void keyCallback(int key, int scan_code, int action, int mods);
    void mouseCallback(int button, int action);
    void scrollCallback(double x_offset, double y_offset);
    void focusCallback(int focused);
    void framebufferResizeCallback(int new_width, int new_height);


    bool _focus = true;
    bool _framebufferResized = false;

    std::string _windowName;
    int _width, _height;

    GLFWwindow* _glfwWindow = nullptr;
    std::unique_ptr<Surface> _surface;

    static OSWindow* window_ptr(GLFWwindow* glfw_window);

    static void setGLFWWindowHints();

    static void staticKeyCallback(GLFWwindow* glfw_window, int key, int scan_code, int action, int mods);
    static void staticMouseCallback(GLFWwindow* glfw_window, int button, int action, int mods);
    static void staticScrollCallback(GLFWwindow* glfw_window, double x_offset, double y_offset);
    static void staticFramebufferResizeCallback(GLFWwindow* glfw_window, int width, int height);
    static void staticFocusCallback(GLFWwindow* glfw_window, int focused);

public:
    [[nodiscard]] bool shouldClose() const { return glfwWindowShouldClose(_glfwWindow); }
    [[nodiscard]] VkExtent2D extent() const { return {static_cast<uint32_t>(_width), static_cast<uint32_t>(_height)}; }
    [[nodiscard]] bool windowResized() const { return _framebufferResized; }
    [[nodiscard]] GLFWwindow* window() const { return _glfwWindow; }
    [[nodiscard]] bool focused() const { return _focus; }
    [[nodiscard]] GLFWwindow* get() const { return _glfwWindow; }

    [[nodiscard]] Surface* surface() const { return _surface.get(); }

  


    OSWindow(const OSWindow& other) = delete;
    OSWindow& operator=(const OSWindow& other) = delete;
    OSWindow(OSWindow&& other) = default;
    OSWindow& operator=(OSWindow&& other) = default; // copy/move operations

#ifdef CONSTANT_DEBUG_MODE
    static void debug_check_not_null(const OSWindow* os_window);
    static void debug_check(const OSWindow* os_window);

#define DEBUG_CHECK_OS_WINDOW(os_window_ptr) OSWindow::debug_check(os_window_ptr)
#define DEBUG_CHECK_OS_WINDOW_NOT_NULL(os_window_ptr) OSWindow::debug_check_not_null(os_window_ptr)

#else
#define DEBUG_CHECK_OS_WINDOW(os_window_ptr) ((void)0)
#define DEBUG_CHECK_OS_WINDOW_NOT_NULL(os_window_ptr) ((void)0)
#endif

};
}
