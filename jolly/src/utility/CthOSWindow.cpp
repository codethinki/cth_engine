#include "jolly/utility/CthOSWindow.hpp"

#include "jvk/base/instance.hpp"
#include "jvk/base/destruction_queue.hpp"
#include "jvk/surface/surface.hpp"
#include "jvk/utility/vk_exceptions.hpp"


#include "jolly/user/HlcInputController.hpp"

#include <cth/io/log.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>



namespace jly {
OSWindow::OSWindow(
    jvk::Instance const& instance,
    jvk::DestructionQueue* destruction_queue,
    std::string_view name,
    glm::uvec2 extent
) : _instance{&instance},
    _destructionQueue{destruction_queue},
    _windowName{name},
    _windowExtent{extent} { initWindow(instance); }

OSWindow::~OSWindow() {
    CTH_STABLE_ERR(
        _surface != nullptr,
        "surface must be retrieved (i have to swap glfw with native windows impl, this is crap"
    )
        std::terminate(); // NOLINT(clang-diagnostic-exceptions)

    _surface = nullptr;
    if(_handle) destroy(); //TEMP use created() instead
}

void OSWindow::destroy(jvk::DestructionQueue* destruction_queue) {
    if(destruction_queue) _destructionQueue = destruction_queue;

    auto const lambda = [handle = _handle.get()] { destroy(handle); };

    if(_destructionQueue) _destructionQueue->push(lambda);
    else lambda();

    //TEMP call reset
    _handle = nullptr;
}

void OSWindow::waitEvents() { glfwWaitEvents(); }

std::vector<std::string> OSWindow::getGLFWInstanceExtensions() {
    uint32_t glfwExtensionCount = 0;
    auto const glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    std::span<char const*> extensionsSpan{glfwExtensions, glfwExtensionCount};
    std::vector<std::string> extensions{extensionsSpan.size()};
    std::ranges::transform(
        extensionsSpan,
        extensions.begin(),
        [](std::string_view c) { return std::string(c); }
    );

    return extensions;
}


void OSWindow::init() {
    glfwInit();
    CTH_STABLE_ERR(glfwVulkanSupported() != VK_TRUE, "GLFW: Vulkan not supported")
        throw details->exception();
    setGLFWWindowHints();

    cth::log::msg("initialized window");
}

void OSWindow::terminate() {
    glfwTerminate();
    cth::log::msg("terminated window");
}

void OSWindow::destroy(GLFWwindow* glfw_window) {
    glfwDestroyWindow(glfw_window);
    cth::log::msg("destroyed window");
}

void OSWindow::createWindow() {
    _handle = glfwCreateWindow(_windowExtent.x, _windowExtent.y, _windowName.c_str(), nullptr, nullptr);
}

void OSWindow::setCallbacks() {
    glfwSetWindowUserPointer(_handle.get(), this);
    glfwSetKeyCallback(_handle.get(), staticKeyCallback);
    glfwSetMouseButtonCallback(_handle.get(), staticMouseCallback);
    glfwSetScrollCallback(_handle.get(), staticScrollCallback);
    glfwSetFramebufferSizeCallback(_handle.get(), staticFramebufferResizeCallback);
    glfwSetWindowSizeCallback(_handle.get(), staticWindowResizeCallback);
    glfwSetWindowFocusCallback(_handle.get(), staticFocusCallback);
    //glfwSetCursorPosCallback(hlcWindow, staticMovementCallback);
}

void OSWindow::createSurface(jvk::Instance const& instance) {
    VkSurfaceKHR vkSurface = VK_NULL_HANDLE;
    auto const result = glfwCreateWindowSurface(instance.get(), _handle.get(), nullptr, &vkSurface);

    _surface = vkSurface;

    CTH_STABLE_ERR(result != VK_SUCCESS, "failed to create GLFW window surface")
        throw jvk::vk_result_exception{result, details->exception()};

    cth::log::msg("created surface");
}

void OSWindow::updateSizes() {
    glm::ivec2 framebufferExtent;
    glfwGetFramebufferSize(_handle.get(), &framebufferExtent.x, &framebufferExtent.y);
    _framebufferExtent = {framebufferExtent.x, framebufferExtent.y};

    glm::ivec2 windowExtent;
    glfwGetWindowSize(_handle.get(), &windowExtent.x, &windowExtent.y);
    _windowExtent = {windowExtent.x, windowExtent.y};
}

void OSWindow::initWindow(jvk::Instance const& instance) {
    createWindow();
    setCallbacks();
    updateSizes();

    createSurface(instance);
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
    CTH_STABLE_ERR(new_width < 0 || new_height < 0, "framebuffer sizes must be >= 0")
        throw cth::except::default_exception{"framebuffer sizes invalid"};

    _framebufferExtent = {new_width, new_height};
}

void OSWindow::windowResizeCallback(int new_width, int new_height) {
    CTH_STABLE_ERR(new_width < 0 || new_height < 0, "window sizes must be >= 0")
        throw cth::except::default_exception{"window sizes invalid"};

    _windowExtent = {new_width, new_height};
}

OSWindow* OSWindow::window_ptr(GLFWwindow* glfw_window) {
    return static_cast<OSWindow*>(glfwGetWindowUserPointer(glfw_window));
}

void OSWindow::setGLFWWindowHints() {
    //const vector windowIcons = loadWindowIcons();
    //glfwSetWindowIcon(hlcWindow, static_cast<int>(windowIcons.size()), windowIcons.data());
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);
    glfwWindowHint(GLFW_DECORATED, GLFW_TRUE);
}

void OSWindow::staticKeyCallback(GLFWwindow* glfw_window, int key, int scan_code, int action, int mods) {
    if(key < 0) return;
    InputController::keyStates[key] = action; //TODO review this
    window_ptr(glfw_window)->keyCallback(key, scan_code, action, mods);
}



void OSWindow::staticMouseCallback(
    GLFWwindow* glfw_window,
    int button,
    int action,
    [[maybe_unused]] int mods
) { window_ptr(glfw_window)->mouseCallback(button, action); }

void OSWindow::staticScrollCallback(GLFWwindow* glfw_window, double x_offset, double y_offset) {
    window_ptr(glfw_window)->scrollCallback(x_offset, y_offset);
}

void OSWindow::staticFramebufferResizeCallback(GLFWwindow* glfw_window, int width, int height) {
    window_ptr(glfw_window)->framebufferResizeCallback(width, height);
}

void OSWindow::staticWindowResizeCallback(handle_t glfw_window, int width, int height) {
    window_ptr(glfw_window)->windowResizeCallback(width, height);
}

void OSWindow::staticFocusCallback(GLFWwindow* glfw_window, int focused) {
    window_ptr(glfw_window)->focusCallback(focused);
}

bool OSWindow::shouldClose() const { return glfwWindowShouldClose(_handle.get()); }

glm::uvec2 OSWindow::framebufferExtent() const {
    glm::ivec2 framebufferExtent;
    glfwGetFramebufferSize(_handle.get(), &framebufferExtent.x, &framebufferExtent.y);
    return {framebufferExtent.x, framebufferExtent.y};
}

}
