#include "cgfw/window/window.hpp"

#include "cgfw/utility/exception.hpp"

#include "cvk/base/instance.hpp"
#include "cvk/utility/format.hpp"

#include <RGFW.h>

#include <span>


namespace cgfw {
namespace {
    constexpr bool same_flag(WindowFlags flag, RGFW_windowFlags rgfwFlag) {
        return static_cast<RGFW_windowFlags>(flag) == rgfwFlag;
    }

    static_assert(same_flag(WindowFlags::NO_BORDER, RGFW_windowNoBorder));
    static_assert(same_flag(WindowFlags::NO_RESIZE, RGFW_windowNoResize));
    static_assert(same_flag(WindowFlags::FULLSCREEN, RGFW_windowFullscreen));
    static_assert(same_flag(WindowFlags::CENTER, RGFW_windowCenter));
    static_assert(same_flag(WindowFlags::SCALE_TO_MONITOR, RGFW_windowScaleToMonitor));
    static_assert(same_flag(WindowFlags::HIDE, RGFW_windowHide));
    static_assert(same_flag(WindowFlags::MAXIMIZE, RGFW_windowMaximize));
    static_assert(same_flag(WindowFlags::FLOATING, RGFW_windowFloating));
    static_assert(same_flag(WindowFlags::FOCUS_ON_SHOW, RGFW_windowFocusOnShow));
    static_assert(same_flag(WindowFlags::MINIMIZE, RGFW_windowMinimize));
}

void dev::window_deleter::operator()(RGFW_window* rgfwWindow) const { Window::destroy(rgfwWindow); }

Window::Window(Config config) : _config{std::move(config)} {}

Window::Window(Config config, State const& state) : Window{std::move(config)} { wrap(state); }

Window::Window(Config config, create_t) : Window{std::move(config)} { create(); }

void Window::wrap(State const& state) {
    optDestroy();

    _handle.reset(state.rgfwWindow.get());
}

void Window::create() {
    Config::debug_check(_config);
    optDestroy();

    auto* const rgfwWindow = RGFW_createWindow(
        _config.title.c_str(),
        0,
        0,
        static_cast<i32>(_config.extent.width),
        static_cast<i32>(_config.extent.height),
        static_cast<RGFW_windowFlags>(_config.flags)
    );
    CGFW_STABLE_THROW(rgfwWindow == nullptr, "failed to create the window {}, e.g. no display", _config.title) {
        forget();
    }

    _handle.reset(rgfwWindow);
}

void Window::destroy() {
    debug_check(*this);

    _handle.reset();

    forget();
}

Window::State Window::release() {
    debug_check(*this);

    State const state{_handle.release()};
    forget();
    return state;
}

cvk::Surface Window::surface(cvk::Instance const& instance) const {
    debug_check(*this);
    cvk::Instance::debug_check(instance);

    VkSurfaceKHR vkSurface = VK_NULL_HANDLE;
    auto const result = RGFW_window_createSurface_Vulkan(_handle.get(), instance.get(), &vkSurface);
    CGFW_STABLE_THROW(
        result != VK_SUCCESS,
        "failed to create the surface of the window {}, got {}",
        _config.title,
        result
    ) {}

    return cvk::Surface{instance, cvk::Surface::State{vkSurface}};
}

void Window::pollEvents() const {
    debug_check(*this);

    RGFW_pollEvents();
}

bool Window::shouldClose() const {
    debug_check(*this);

    return RGFW_window_shouldClose(_handle.get()) != RGFW_FALSE;
}

void Window::destroy(RGFW_window* rgfwWindow) {
    CTH_CRITICAL(rgfwWindow == nullptr, "rgfwWindow must not be nullptr") {}

    RGFW_window_close(rgfwWindow);
}

void Window::forget() {}

std::vector<std::string> instance_extensions() {
    size_t count = 0;
    char const** const extensions = RGFW_getRequiredInstanceExtensions_Vulkan(&count);

    return std::vector<std::string>{std::from_range, std::span{extensions, count}};
}
}
