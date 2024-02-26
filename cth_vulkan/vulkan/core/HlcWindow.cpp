import hlc.graphics;
import hlc.math;
#include "HlcWindow.hpp"

#include "../user/HlcInputController.hpp"
#include "utils/images.hpp"

#include <iostream>
#include <stdexcept>


namespace cth {
using std::vector;
Window::Window(const int w, const int h, const std::string& name) : windowName{name} {
    width = w;
    height = h;
	initWindow();

}
Window::~Window() {
    glfwDestroyWindow(hlcWindow);
    glfwTerminate();
    userPointer = nullptr;
}

void Window::initWindow() {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);
    glfwWindowHint(GLFW_DECORATED, GLFW_TRUE);
    hlcWindow = glfwCreateWindow(width, height, windowName.c_str(), nullptr, nullptr);;
    glfwGetWindowSize(hlcWindow, &width, &height);

    //const vector windowIcons = loadWindowIcons();
    //glfwSetWindowIcon(hlcWindow, static_cast<int>(windowIcons.size()), windowIcons.data());

    glfwSetWindowUserPointer(hlcWindow, this);
    glfwSetKeyCallback(hlcWindow, staticKeyCallback);
    glfwSetMouseButtonCallback(hlcWindow, staticMouseCallback);
    glfwSetScrollCallback(hlcWindow, staticScrollCallback);
    glfwSetFramebufferSizeCallback(hlcWindow, framebufferResizeCallback);
    glfwSetWindowFocusCallback(hlcWindow, staticFocusCallback);
    //glfwSetCursorPosCallback(hlcWindow, staticMovementCallback);
    userPointer = static_cast<Window*>(glfwGetWindowUserPointer(hlcWindow));
}

void Window::createWindowSurface(VkInstance instance, VkSurfaceKHR* surface) const {
    if(glfwCreateWindowSurface(instance, hlcWindow, nullptr, surface) != VK_SUCCESS)
        throw std::runtime_error(
            "createWindowSurface: failed to create window surface");
}

void Window::staticKeyCallback(GLFWwindow* window, const int key, int scan_code, const int action, int mods) {
    if(key < 0) return;
	InputController::keyStates[key] = action;
}

void Window::staticMouseCallback(GLFWwindow* window, const int button, const int action, int mods) {
    const auto newHlcWindow = static_cast<Window*>(glfwGetWindowUserPointer(window));
    newHlcWindow->mouseCallback(button, action);
}
void Window::mouseCallback(const int button, const int action) {} //FEATURE mouse callback

void Window::staticScrollCallback(GLFWwindow* window, const double x_offset, const double y_offset) { userPointer->scrollCallback(x_offset, y_offset); }
void Window::scrollCallback(double x_offset, double y_offset) { } //FEATURE scroll callback

void Window::framebufferResizeCallback(GLFWwindow* window, const int w, const int h) {
	framebufferResized = true;
    width = w;
    height = h;

    cout << "hehehe" << '\n';
}
void Window::staticFocusCallback(GLFWwindow* window, const int focused) {
    double x = 0, y = 0;
    glfwGetCursorPos(window, &x, &y);
    if(focused && !framebufferResized && math::between2d<int>(static_cast<int>(x), 0,
        width, static_cast<int>(y), 0, height)) {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        focus = true;
    } else {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        focus = false;
    }
}

vector<GLFWimage> Window::loadWindowIcons() const {
    vector<GLFWimage> icons;
    for(int i = 256; i >= 16; i /= 2) icons.push_back(loadGLFWImage("resources/window_icons/icon" + to_string(i) + ".png"));
    return icons;
}
GLFWimage Window::loadGLFWImage(const std::string& path) {
    int x, y, channels;
    unsigned char* image = img::load(path, x, y, channels);
    return {x, y, image};
}
}
