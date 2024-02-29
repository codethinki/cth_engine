#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <string>

namespace cth {
using namespace std;

class Window {
public:
    Window(string name, uint32_t width, uint32_t height);
    ~Window();

    /**
     * \throws cth::except::data_exception data: VkResult of glfwCreateWindowSurface() 
     */
    void createWindowSurface(VkInstance instance, VkSurfaceKHR* surface) const;
    void resetWindowResized() { framebufferResized = false; }

private:
    void setGLFWWindowSettings();
    void setCallbacks();
    void initWindow();



    void keyCallback(int key, int scan_code, int action, int mods);
    void mouseCallback(int button, int action);
    void scrollCallback(double x_offset, double y_offset);
    void focusCallback(int focused);
    void framebufferResizeCallback(int new_width, int new_height);

    static Window* window_ptr(GLFWwindow* glfw_window);
    static void staticKeyCallback(GLFWwindow* window, int key, int scan_code, int action, int mods);
    static void staticMouseCallback(GLFWwindow* glfw_window, int button, int action, int mods);
    static void staticScrollCallback(GLFWwindow* glfw_window, double x_offset, double y_offset);
    static void staticFramebufferResizeCallback(GLFWwindow* glfw_window, int width, int height);
    static void staticFocusCallback(GLFWwindow* glfw_window, int focused);

    bool focus = true;
    bool framebufferResized = false;

    int width, height;
    std::string windowName;
    GLFWwindow* glfwWindow;
public:
    [[nodiscard]] bool shouldClose() const { return glfwWindowShouldClose(glfwWindow); }
    [[nodiscard]] VkExtent2D getExtent() const { return {static_cast<uint32_t>(width), static_cast<uint32_t>(height)}; }
    [[nodiscard]] bool windowResized() const { return framebufferResized; }
    [[nodiscard]] GLFWwindow* window() const { return glfwWindow; }
    [[nodiscard]] bool focused() const { return focus; }

    Window(const Window& other) = delete;
    Window& operator=(const Window& other) = delete;
    Window(Window&& other) = default;
    Window& operator=(Window&& other) = default; // copy/move operations
};
}
