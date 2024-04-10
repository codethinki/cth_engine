#pragma once
#define GLFW_INCLUDE_VULKAN
#include <memory>
#include <GLFW/glfw3.h>

#include <string>
#include <vector>

namespace cth {
class Instance;
class Surface;

using namespace std;

class Window {
public:
    Window(string_view name, uint32_t width, uint32_t height, Instance* instance);
    ~Window();

    void resetWindowResized() { framebufferResized = false; }

private:
    void initWindow();
    void setCallbacks();
    void createSurface(Instance* instance);


    void keyCallback(int key, int scan_code, int action, int mods);
    void mouseCallback(int button, int action);
    void scrollCallback(double x_offset, double y_offset);
    void focusCallback(int focused);
    void framebufferResizeCallback(int new_width, int new_height);


    bool focus = true;
    bool framebufferResized = false;

    string windowName;
    int width, height;

    GLFWwindow* glfwWindow = nullptr;
    unique_ptr<Surface> _surface;

    static Window* window_ptr(GLFWwindow* glfw_window);

    static void setGLFWWindowHints();

    static void staticKeyCallback(GLFWwindow* window, int key, int scan_code, int action, int mods);
    static void staticMouseCallback(GLFWwindow* glfw_window, int button, int action, int mods);
    static void staticScrollCallback(GLFWwindow* glfw_window, double x_offset, double y_offset);
    static void staticFramebufferResizeCallback(GLFWwindow* glfw_window, int width, int height);
    static void staticFocusCallback(GLFWwindow* glfw_window, int focused);

public:
    [[nodiscard]] bool shouldClose() const { return glfwWindowShouldClose(glfwWindow); }
    [[nodiscard]] VkExtent2D extent() const { return {static_cast<uint32_t>(width), static_cast<uint32_t>(height)}; }
    [[nodiscard]] bool windowResized() const { return framebufferResized; }
    [[nodiscard]] GLFWwindow* window() const { return glfwWindow; }
    [[nodiscard]] bool focused() const { return focus; }
    [[nodiscard]] GLFWwindow* get() const { return glfwWindow; }

    [[nodiscard]] Surface* surface() const { return _surface.get(); }

    static vector<string> getGLFWInstanceExtensions();
    static void init();
    static void terminate();

    Window(const Window& other) = delete;
    Window& operator=(const Window& other) = delete;
    Window(Window&& other) = default;
    Window& operator=(Window&& other) = default; // copy/move operations
};
}
