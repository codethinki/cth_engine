#include "CthWindow.hpp"

#include "../user/HlcInputController.hpp"
#include "../utils/cth_vk_specific_utils.hpp"

#include <cth/cth_log.hpp>
#include <cth/cth_numeric.hpp>

#include <iostream>
#include <stdexcept>


namespace cth {
Window::Window(std::string name, const uint32_t width, const uint32_t height) : width(static_cast<int>(width)), height(static_cast<int>(height)),
    windowName{std::move(name)} { initWindow(); }
Window::~Window() {
    glfwDestroyWindow(glfwWindow);
    glfwTerminate();
}

void Window::setGLFWWindowSettings() {
    //const vector windowIcons = loadWindowIcons();
    //glfwSetWindowIcon(hlcWindow, static_cast<int>(windowIcons.size()), windowIcons.data());
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);
    glfwWindowHint(GLFW_DECORATED, GLFW_TRUE);
}
void Window::setCallbacks() {


    glfwSetWindowUserPointer(glfwWindow, this);
    glfwSetKeyCallback(glfwWindow, staticKeyCallback);
    glfwSetMouseButtonCallback(glfwWindow, staticMouseCallback);
    glfwSetScrollCallback(glfwWindow, staticScrollCallback);
    glfwSetFramebufferSizeCallback(glfwWindow, staticFramebufferResizeCallback);
    glfwSetWindowFocusCallback(glfwWindow, staticFocusCallback);
    //glfwSetCursorPosCallback(hlcWindow, staticMovementCallback);
}
void Window::initWindow() {
    glfwInit();

    glfwWindow = glfwCreateWindow(width, height, windowName.c_str(), nullptr, nullptr);
    glfwGetWindowSize(glfwWindow, &width, &height);
}

void Window::createWindowSurface(VkInstance instance, VkSurfaceKHR* surface) const {
    const VkResult result = glfwCreateWindowSurface(instance, glfwWindow, nullptr, surface);

    CTH_STABLE_ERR(result == VK_SUCCESS, "Vk: failed to create GLFW window surface")
        throw cth::except::vk_result_exception{result, details->exception()};
}


void Window::keyCallback(int key, int scan_code, int action, int mods) {}
void Window::mouseCallback(const int button, const int action) {} //FEATURE mouse callback
void Window::scrollCallback(double x_offset, double y_offset) {} //FEATURE scroll callback
void Window::focusCallback(const int focused) {
    focus = static_cast<bool>(focused);

    if(focus) cth::log::msg(except::LOG, "window focused");
    else cth::log::msg(except::LOG, "window unfocused");

    //TODO review this later and probably move it into an app function
    /*double x = 0, y = 0;
    glfwGetCursorPos(glfwWindow, &x, &y);
    if(focused && !framebufferResized && cth::num::inRange2d(x, 0.0, static_cast<double>(width), y, 0.0, static_cast<double>(height))) {
        glfwSetInputMode(glfwWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        focus = true;
    } else {
        glfwSetInputMode(glfwWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        focus = false;
    }*/
}
void Window::framebufferResizeCallback(const int new_width, const int new_height) {
    framebufferResized = true;
    width = new_width;
    height = new_height;

    cth::log::msg(except::INFO, "framebuffer resized to {0}x{1}", new_width, new_height);
}


}

//static methods
namespace cth {
Window* Window::window_ptr(GLFWwindow* glfw_window) { return static_cast<Window*>(glfwGetWindowUserPointer(glfw_window)); }

void Window::staticKeyCallback(GLFWwindow* glfw_window, const int key, const int scan_code, const int action, const int mods) {
    if(key < 0) return;
    InputController::keyStates[key] = action; //TODO review this
    window_ptr(glfw_window)->keyCallback(key, scan_code, action, mods);
}
void Window::staticMouseCallback(GLFWwindow* glfw_window, const int button, const int action, int mods) {
    window_ptr(glfw_window)->mouseCallback(button, action);
}
void Window::staticScrollCallback(GLFWwindow* glfw_window, const double x_offset, const double y_offset) {
    window_ptr(glfw_window)->scrollCallback(x_offset, y_offset);
}
void Window::staticFramebufferResizeCallback(GLFWwindow* glfw_window, const int width, const int height) {
    window_ptr(glfw_window)->framebufferResizeCallback(width, height);
}
void Window::staticFocusCallback(GLFWwindow* glfw_window, const int focused) { window_ptr(glfw_window)->focusCallback(focused); }
}
