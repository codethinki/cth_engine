#pragma once
#define GLFW_INCLUDE_VULKAN
#include <string>
#include <vector>
#include <GLFW/glfw3.h>


namespace cth {

using namespace std;

class Window {
public:
	Window(int w, int h, const std::string& name);
	~Window();

    void createWindowSurface(VkInstance instance, VkSurfaceKHR* surface) const;

	static void resetWindowResized() { framebufferResized = false; }




    [[nodiscard]] bool shouldClose() const { return glfwWindowShouldClose(hlcWindow); }
    [[nodiscard]] static VkExtent2D getExtent() { return {static_cast<uint32_t>(width), static_cast<uint32_t>(height)}; }
    [[nodiscard]] static bool windowResized() { return framebufferResized; }
    [[nodiscard]] GLFWwindow* window() const { return hlcWindow; }
    [[nodiscard]] static bool focused() { return focus; }

    Window(const Window& other) = delete;
    Window& operator=(const Window& other) = delete;
    Window(Window&& other) = default;
    Window& operator=(Window&& other) = default;// copy/move operations

private:
    static void framebufferResizeCallback(GLFWwindow* window, int w, int h);

	static void staticKeyCallback(GLFWwindow* window, int key, int scan_code, int action, int mods);

	void mouseCallback(int button, int action);
	static void staticMouseCallback(GLFWwindow* window, int button, int action, int mods);
	
	void scrollCallback(double x_offset, double y_offset);
	static void staticScrollCallback(GLFWwindow* window, double x_offset, double y_offset);
	
	static void staticFocusCallback(GLFWwindow* window, int focused);

    [[nodiscard]] vector<GLFWimage> loadWindowIcons() const;
	static GLFWimage loadGLFWImage(const std::string& path);

	inline static bool focus = true;
	inline static bool framebufferResized = false;
	inline static int width = 0, height = 0;

	inline static Window* userPointer;

	std::string windowName;
	GLFWwindow* hlcWindow;

	void initWindow();
};
}
