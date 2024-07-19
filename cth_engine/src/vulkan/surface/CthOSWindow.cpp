#include "CthOSWindow.hpp"

#include "CthSurface.hpp"
#include "interface/user/HlcInputController.hpp"
#include "vulkan/base/CthInstance.hpp"
#include "vulkan/utility/CthVkUtils.hpp"


#include <vulkan/vulkan_win32.h>


namespace cth::vk {
OSWindow::OSWindow(const std::string_view name, const uint32_t width, const uint32_t height, const BasicInstance* instance) : _windowName{name},
    _width(static_cast<int>(width)), _height(static_cast<int>(height)) {
    initWindow();

    setCallbacks();

    createSurface(instance);
}
OSWindow::~OSWindow() {
    _surface = nullptr;

    glfwDestroyWindow(_glfwWindow);
    cth::log::msg("destroyed window");
}



void OSWindow::initWindow() {
    _glfwWindow = glfwCreateWindow(_width, _height, _windowName.c_str(), nullptr, nullptr);
    glfwGetWindowSize(_glfwWindow, &_width, &_height);
}
void OSWindow::setCallbacks() {
    glfwSetWindowUserPointer(_glfwWindow, this);
    glfwSetKeyCallback(_glfwWindow, staticKeyCallback);
    glfwSetMouseButtonCallback(_glfwWindow, staticMouseCallback);
    glfwSetScrollCallback(_glfwWindow, staticScrollCallback);
    glfwSetFramebufferSizeCallback(_glfwWindow, staticFramebufferResizeCallback);
    glfwSetWindowFocusCallback(_glfwWindow, staticFocusCallback);
    //glfwSetCursorPosCallback(hlcWindow, staticMovementCallback);
}
void OSWindow::createSurface(const BasicInstance* instance) {
    VkSurfaceKHR vkSurface = VK_NULL_HANDLE;
    const auto result = glfwCreateWindowSurface(instance->get(), window(), nullptr, &vkSurface);

    CTH_STABLE_ERR(result != VK_SUCCESS, "failed to create GLFW window surface")
        throw cth::except::vk_result_exception{result, details->exception()};

    _surface = std::make_unique<Surface>(instance, vkSurface);

    cth::log::msg("created surface");

}

void OSWindow::keyCallback(int key, int scan_code, int action, int mods) {}
void OSWindow::mouseCallback(const int button, const int action) {} //FEATURE mouse callback
void OSWindow::scrollCallback(double x_offset, double y_offset) {} //FEATURE scroll callback
void OSWindow::focusCallback(const int focused) {
    _focus = static_cast<bool>(focused);

    if(_focus) cth::log::msg("window focused");
    else cth::log::msg("window unfocused");

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
void OSWindow::framebufferResizeCallback(const int new_width, const int new_height) {
    _framebufferResized = true;
    _width = new_width;
    _height = new_height;
}


}


//static methods

namespace cth::vk {
void OSWindow::setGLFWWindowHints() {
    //const vector windowIcons = loadWindowIcons();
    //glfwSetWindowIcon(hlcWindow, static_cast<int>(windowIcons.size()), windowIcons.data());
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);
    glfwWindowHint(GLFW_DECORATED, GLFW_TRUE);
}
OSWindow* OSWindow::window_ptr(GLFWwindow* glfw_window) { return static_cast<OSWindow*>(glfwGetWindowUserPointer(glfw_window)); }
std::vector<std::string> OSWindow::getGLFWInstanceExtensions() {
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    std::span<const char*> extensionsSpan{glfwExtensions, glfwExtensionCount};
    std::vector<std::string> extensions{extensionsSpan.size()};
    std::ranges::transform(extensionsSpan, extensions.begin(), [](const std::string_view c) { return std::string(c); });

    return extensions;
}
void OSWindow::init() {
    glfwInit();
    CTH_STABLE_ERR(glfwVulkanSupported() != VK_TRUE, "GLFW: Vulkan not supported")
        throw details->exception();
    setGLFWWindowHints();

    log::msg("initialized window");
}

void OSWindow::terminate() {
    glfwTerminate();
    log::msg("terminated window");
}
VkSurfaceKHR OSWindow::tempSurface(const BasicInstance* instance) {
    VkSurfaceKHR surface = VK_NULL_HANDLE;

    // Create a hidden window for the surface
    constexpr std::wstring_view name = L"TempHiddenWindow";
    const WNDCLASSEX wc{
        .cbSize = sizeof(wc),
        .lpfnWndProc = DefWindowProc,
        .hInstance = GetModuleHandle(nullptr),
        .lpszClassName = name.data(),
    };
    RegisterClassEx(&wc);

    HWND hwnd = CreateWindowEx(0, name.data(), L"TempHiddenSurface", 0, 0, 0, 0, 0, nullptr, nullptr, GetModuleHandle(nullptr), nullptr);

    CTH_STABLE_ERR(hwnd == nullptr, "failed to create temp window")
        throw details->exception();


    // Create the Vulkan surface
    VkWin32SurfaceCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    createInfo.hinstance = GetModuleHandle(nullptr);
    createInfo.hwnd = hwnd;

    const auto result = vkCreateWin32SurfaceKHR(instance->get(), &createInfo, nullptr, &surface);
    CTH_STABLE_ERR(result != VK_SUCCESS, "failed to create temp surface") {
        DestroyWindow(hwnd);
        throw except::vk_result_exception{result, details->exception()};
    }

    log::msg("created temp surface");

    return surface;
}

#ifdef CONSTANT_DEBUG_MODE
void OSWindow::debug_check_not_null(const OSWindow* os_window) {
    CTH_ERR(os_window == nullptr, "os_window must not be nullptr")
        throw details->exception();
}
void OSWindow::debug_check(const OSWindow* os_window) {
    DEBUG_CHECK_OS_WINDOW_NOT_NULL(os_window);

    CTH_ERR(os_window->_glfwWindow == nullptr, "os_window must be initialized")
        throw details->exception();
}
#endif



void OSWindow::staticKeyCallback(GLFWwindow* glfw_window, const int key, const int scan_code, const int action, const int mods) {
    if(key < 0) return;
    InputController::keyStates[key] = action; //TODO review this
    window_ptr(glfw_window)->keyCallback(key, scan_code, action, mods);
}
void OSWindow::staticMouseCallback(GLFWwindow* glfw_window, const int button, const int action, int mods) {
    window_ptr(glfw_window)->mouseCallback(button, action);
}
void OSWindow::staticScrollCallback(GLFWwindow* glfw_window, const double x_offset, const double y_offset) {
    window_ptr(glfw_window)->scrollCallback(x_offset, y_offset);
}
void OSWindow::staticFramebufferResizeCallback(GLFWwindow* glfw_window, const int width, const int height) {
    window_ptr(glfw_window)->framebufferResizeCallback(width, height);
}
void OSWindow::staticFocusCallback(GLFWwindow* glfw_window, const int focused) { window_ptr(glfw_window)->focusCallback(focused); }



} // namespace cth
