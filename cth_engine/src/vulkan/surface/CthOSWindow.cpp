#include "CthOSWindow.hpp"

#include "CthSurface.hpp"
#include "interface/user/HlcInputController.hpp"
#include "vulkan/base/CthInstance.hpp"
#include "vulkan/resource/CthDestructionQueue.hpp"
#include "vulkan/utility/cth_vk_utils.hpp"


#include <vulkan/vulkan_win32.h>


namespace cth::vk {
OSWindow::OSWindow(BasicInstance const* instance, DestructionQueue* destruction_queue, std::string_view name, VkExtent2D extent) :
    _instance{instance}, _destructionQueue{destruction_queue}, _windowName{name},
    _width{static_cast<int>(extent.width)}, _height{static_cast<int>(extent.height)} {
    initWindow();

    setCallbacks();

    createSurface(instance);
}
OSWindow::~OSWindow() {
    CTH_STABLE_ERR(_surface != nullptr, "surface must be retrieved (i have to swap glfw with native windows impl, this is crap")
        throw details->exception(); // NOLINT(clang-diagnostic-exceptions)

    if(_surface) {
        Surface::destroy(_surface.get(), _instance->get());
        _surface = nullptr;
    }
    if(_handle) destroy();
}
void OSWindow::destroy(DestructionQueue* destruction_queue) {
    if(destruction_queue) _destructionQueue = destruction_queue;

    if(_destructionQueue) _destructionQueue->push(_handle.get());
    else destroy(_handle.get());

    _handle = nullptr;
}


void OSWindow::initWindow() {
    _handle = glfwCreateWindow(_width, _height, _windowName.c_str(), nullptr, nullptr);
    glfwGetWindowSize(_handle.get(), &_width, &_height);
}
void OSWindow::setCallbacks() {
    glfwSetWindowUserPointer(_handle.get(), this);
    glfwSetKeyCallback(_handle.get(), staticKeyCallback);
    glfwSetMouseButtonCallback(_handle.get(), staticMouseCallback);
    glfwSetScrollCallback(_handle.get(), staticScrollCallback);
    glfwSetFramebufferSizeCallback(_handle.get(), staticFramebufferResizeCallback);
    glfwSetWindowFocusCallback(_handle.get(), staticFocusCallback);
    //glfwSetCursorPosCallback(hlcWindow, staticMovementCallback);
}
void OSWindow::createSurface(BasicInstance const* instance) {
    VkSurfaceKHR vkSurface = VK_NULL_HANDLE;
    auto const result = glfwCreateWindowSurface(instance->get(), window(), nullptr, &vkSurface);

    _surface = vkSurface;

    CTH_STABLE_ERR(result != VK_SUCCESS, "failed to create GLFW window surface")
        throw cth::except::vk_result_exception{result, details->exception()};

    cth::log::msg("created surface");

}

void OSWindow::keyCallback(int key, int scan_code, int action, int mods) {}
void OSWindow::mouseCallback(int button, int action) {} //FEATURE mouse callback
void OSWindow::scrollCallback(double x_offset, double y_offset) {} //FEATURE scroll callback
void OSWindow::focusCallback(int focused) {
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
void OSWindow::framebufferResizeCallback(int new_width, int new_height) {
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
    char const** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    std::span<char const*> extensionsSpan{glfwExtensions, glfwExtensionCount};
    std::vector<std::string> extensions{extensionsSpan.size()};
    std::ranges::transform(extensionsSpan, extensions.begin(), [](std::string_view c) { return std::string(c); });

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
VkSurfaceKHR OSWindow::tempSurface(BasicInstance const* instance) {
    VkSurfaceKHR surface = VK_NULL_HANDLE;

    // Create a hidden window for the surface
    constexpr std::wstring_view name = L"TempHiddenWindow";
    WNDCLASSEX const wc{
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

    auto const result = vkCreateWin32SurfaceKHR(instance->get(), &createInfo, nullptr, &surface);
    CTH_STABLE_ERR(result != VK_SUCCESS, "failed to create temp surface") {
        DestroyWindow(hwnd);
        throw except::vk_result_exception{result, details->exception()};
    }

    log::msg("created temp surface");

    return surface;
}
void OSWindow::destroy(GLFWwindow* glfw_window) {
    glfwDestroyWindow(glfw_window);
    cth::log::msg("destroyed window");
}

#ifdef CONSTANT_DEBUG_MODE
void OSWindow::debug_check_not_null(OSWindow const* os_window) {
    CTH_ERR(os_window == nullptr, "os_window must not be nullptr")
        throw details->exception();
}
void OSWindow::debug_check(OSWindow const* os_window) {
    DEBUG_CHECK_OS_WINDOW_NOT_NULL(os_window);

    CTH_ERR(os_window->_handle == nullptr, "os_window must be initialized")
        throw details->exception();
}
#endif



void OSWindow::staticKeyCallback(GLFWwindow* glfw_window, int key, int scan_code, int action, int mods) {
    if(key < 0) return;
    InputController::keyStates[key] = action; //TODO review this
    window_ptr(glfw_window)->keyCallback(key, scan_code, action, mods);
}
void OSWindow::staticMouseCallback(GLFWwindow* glfw_window, int button, int action, int mods) {
    window_ptr(glfw_window)->mouseCallback(button, action);
}
void OSWindow::staticScrollCallback(GLFWwindow* glfw_window, double x_offset, double y_offset) {
    window_ptr(glfw_window)->scrollCallback(x_offset, y_offset);
}
void OSWindow::staticFramebufferResizeCallback(GLFWwindow* glfw_window, int width, int height) {
    window_ptr(glfw_window)->framebufferResizeCallback(width, height);
}
void OSWindow::staticFocusCallback(GLFWwindow* glfw_window, int focused) { window_ptr(glfw_window)->focusCallback(focused); }



} // namespace cth
