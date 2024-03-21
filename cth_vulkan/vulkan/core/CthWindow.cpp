#include "CthWindow.hpp"

#include "CthInstance.hpp"
#include "../user/HlcInputController.hpp"
#include "../utils/cth_vk_specific_utils.hpp"

#include <cth/cth_log.hpp>
#include <cth/cth_numeric.hpp>

#include <iostream>
#include <stdexcept>


namespace cth {



vector<VkPresentModeKHR> Window::presentModes(VkPhysicalDevice physical_device) const {
    uint32_t size;
    const VkResult result1 = vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, vkSurface, &size, nullptr);
    CTH_STABLE_ERR(result1 != VK_SUCCESS, "device-surface present modes query failed")
        throw cth::except::vk_result_exception{result1, details->exception()};

    if(!size) return {};

    vector<VkPresentModeKHR> modes(size);
    const VkResult result2 = vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, vkSurface, &size, modes.data());
    CTH_STABLE_ERR(result2 != VK_SUCCESS, "device-surface present modes query failed")
        throw cth::except::vk_result_exception{result2, details->exception()};

    return modes;
}
vector<VkSurfaceFormatKHR> Window::surfaceFormats(VkPhysicalDevice physical_device) const {
    uint32_t size;
    const VkResult result1 = vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, vkSurface, &size, nullptr);

    CTH_STABLE_ERR(result1 != VK_SUCCESS, "device-surface formats query failed")
        throw except::vk_result_exception{result1, details->exception()};

    if(!size) return {};
    vector<VkSurfaceFormatKHR> formats(size);
    const VkResult result2 = vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, vkSurface, &size, formats.data());
    CTH_STABLE_ERR(result2 != VK_SUCCESS, "device-surface formats query failed")
        throw except::vk_result_exception{result2, details->exception()};

    return formats;
}
VkSurfaceCapabilitiesKHR Window::surfaceCapabilities(VkPhysicalDevice physical_device) const {
    VkSurfaceCapabilitiesKHR cap;
    const auto result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, vkSurface, &cap);

    CTH_STABLE_ERR(result != VK_SUCCESS, "device-surface capabilities query failed")
        throw except::vk_result_exception{result, details->exception()};

    return cap;
}
VkBool32 Window::surfaceSupport(VkPhysicalDevice physical_device, const uint32_t family_index) const {
    VkBool32 support;
    const VkResult result = vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, family_index, vkSurface, &support);
    CTH_STABLE_ERR(result != VK_SUCCESS, "device-surface support query failed")
        throw except::vk_result_exception{result, details->exception()};


    return support;
}

void Window::setGLFWWindowHints() {
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
    glfwWindow = glfwCreateWindow(width, height, windowName.c_str(), nullptr, nullptr);
    glfwGetWindowSize(glfwWindow, &width, &height);
}
void Window::createSurface() {
    const auto result = glfwCreateWindowSurface(instance->get(), glfwWindow, nullptr, &vkSurface);

    CTH_STABLE_ERR(result != VK_SUCCESS, "failed to create GLFW window surface")
        throw cth::except::vk_result_exception{result, details->exception()};
}



void Window::keyCallback(int key, int scan_code, int action, int mods) {}
void Window::mouseCallback(const int button, const int action) {} //FEATURE mouse callback
void Window::scrollCallback(double x_offset, double y_offset) {} //FEATURE scroll callback
void Window::focusCallback(const int focused) {
    focus = static_cast<bool>(focused);

    if(focus) cth::log::msg<except::LOG>("window focused");
    else cth::log::msg<except::LOG>("window unfocused");

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

    cth::log::msg<except::INFO>("framebuffer resized to {0}x{1}", new_width, new_height);
}


}


//static methods

namespace cth {
Window* Window::window_ptr(GLFWwindow* glfw_window) { return static_cast<Window*>(glfwGetWindowUserPointer(glfw_window)); }
vector<string> Window::getGLFWInstanceExtensions() {
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    span<const char*> extensionsSpan{glfwExtensions, glfwExtensionCount};
    vector<string> extensions{extensionsSpan.size()};
    ranges::transform(extensionsSpan, extensions.begin(), [](const string_view c) { return string(c); });

    return extensions;
}
void Window::init() {
    glfwInit();
    CTH_STABLE_ERR(glfwVulkanSupported() != VK_TRUE, "GLFW: Vulkan not supported")
        throw details->exception();
    setGLFWWindowHints();

    log::msg("initialized window");
}

void Window::terminate() {
    glfwTerminate();
    log::msg("terminated window");
}



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

Window::Window(Instance* instance, std::string name, const uint32_t width, const uint32_t height) : windowName{std::move(name)},
    width(static_cast<int>(width)), height(static_cast<int>(height)),
    instance(instance) {
    initWindow();

    setCallbacks();

    createSurface();
}
Window::~Window() {
    vkDestroySurfaceKHR(instance->get(), vkSurface, nullptr);
    log::msg("destroyed surface");

    glfwDestroyWindow(glfwWindow);
    cth::log::msg("destroyed window");
}

} // namespace cth
