#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <string>
#include <vector>

namespace cth {
using namespace std;
class Instance;

class Window {
public:
    void resetWindowResized() { framebufferResized = false; }

    [[nodiscard]] vector<VkPresentModeKHR> presentModes(VkPhysicalDevice physical_device) const;
    [[nodiscard]] vector<VkSurfaceFormatKHR> surfaceFormats(VkPhysicalDevice physical_device) const;
    [[nodiscard]] VkSurfaceCapabilitiesKHR surfaceCapabilities(VkPhysicalDevice physical_device) const;
    [[nodiscard]] VkBool32 surfaceSupport(VkPhysicalDevice physical_device, uint32_t family_index) const;

private:
    static void setGLFWWindowHints();
    void setCallbacks();
    void initWindow();
    void createSurface();


    void keyCallback(int key, int scan_code, int action, int mods);
    void mouseCallback(int button, int action);
    void scrollCallback(double x_offset, double y_offset);
    void focusCallback(int focused);
    void framebufferResizeCallback(int new_width, int new_height);


    bool focus = true;
    bool framebufferResized = false;

    std::string windowName;
    int width, height;

    Instance* instance;
    GLFWwindow* glfwWindow = nullptr;
    VkSurfaceKHR vkSurface = VK_NULL_HANDLE;

    static Window* window_ptr(GLFWwindow* glfw_window);

    static void staticKeyCallback(GLFWwindow* window, int key, int scan_code, int action, int mods);
    static void staticMouseCallback(GLFWwindow* glfw_window, int button, int action, int mods);
    static void staticScrollCallback(GLFWwindow* glfw_window, double x_offset, double y_offset);
    static void staticFramebufferResizeCallback(GLFWwindow* glfw_window, int width, int height);
    static void staticFocusCallback(GLFWwindow* glfw_window, int focused);

public:
    Window(Instance* instance, string name, uint32_t width, uint32_t height);
    ~Window();

    [[nodiscard]] bool shouldClose() const { return glfwWindowShouldClose(glfwWindow); }
    [[nodiscard]] VkExtent2D getExtent() const { return {static_cast<uint32_t>(width), static_cast<uint32_t>(height)}; }
    [[nodiscard]] bool windowResized() const { return framebufferResized; }
    [[nodiscard]] GLFWwindow* window() const { return glfwWindow; }
    [[nodiscard]] bool focused() const { return focus; }
    [[nodiscard]] VkSurfaceKHR surface() const {return vkSurface; }

    static vector<string> getGLFWInstanceExtensions();
    static void init();
    static void terminate();

    Window(const Window& other) = delete;
    Window& operator=(const Window& other) = delete;
    Window(Window&& other) = default;
    Window& operator=(Window&& other) = default; // copy/move operations
};
}
