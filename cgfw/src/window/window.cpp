#include "cgfw/window/window.hpp"

#include "cgfw/utility/exception.hpp"
#include "window/rgfw_event.hpp"
#include "window/rgfw_info.h"

#include "cvk/base/instance.hpp"
#include "cvk/utility/format.hpp"

#include <RGFW.h>

#include <cth/os/osdef.hpp>

#ifdef CTH_PLATFORM_WINDOWS
#include <windows.h>
#endif

#include <algorithm>
#include <span>


namespace cgfw {
namespace {
    constexpr bool same_flag(WindowFlags flag, RGFW_windowFlags rgfwFlag) {
        return static_cast<RGFW_windowFlags>(flag) == rgfwFlag;
    }

    static_assert(same_flag(WindowFlags::NO_BORDER, RGFW_windowNoBorder));
    static_assert(same_flag(WindowFlags::NO_RESIZE, RGFW_windowNoResize));
    static_assert(same_flag(WindowFlags::FULLSCREEN, RGFW_windowFullscreenBorderless));
    static_assert(same_flag(WindowFlags::CENTER, RGFW_windowCenter));
    static_assert(same_flag(WindowFlags::SCALE_TO_MONITOR, RGFW_windowScaleToMonitor));
    static_assert(same_flag(WindowFlags::HIDE, RGFW_windowHide));
    static_assert(same_flag(WindowFlags::MAXIMIZE, RGFW_windowMaximize));
    static_assert(same_flag(WindowFlags::FLOATING, RGFW_windowFloating));
    static_assert(same_flag(WindowFlags::FOCUS_ON_SHOW, RGFW_windowFocusOnShow));
    static_assert(same_flag(WindowFlags::MINIMIZE, RGFW_windowMinimize));

    /**
     * deinitializes RGFW if it is initialized and has no window
     */
    void deinit_without_windows() {
        if(RGFW_getInfo() != nullptr && cgfw_rgfw_window_count() == 0)
            RGFW_deinit();
    }
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

    // RGFW_initVulkan: RGFW_window_createSurface_Vulkan() needs the Vulkan library of RGFW
    if(RGFW_getInfo() == nullptr) {
        auto const initResult = RGFW_init(nullptr, RGFW_initVulkan);
        CGFW_STABLE_THROW(
            initResult < 0,
            "failed to initialize RGFW for the window {}, e.g. no display, got {}",
            _config.title,
            initResult
        ) { forget(); }

        // the queue holds the events between two pollEvents(), also the first ones of a window
        RGFW_setQueueEvents(RGFW_TRUE);
    }

    auto* const rgfwWindow = RGFW_createWindow(
        _config.title.c_str(),
        0,
        0,
        static_cast<i32>(_config.extent.width),
        static_cast<i32>(_config.extent.height),
        static_cast<RGFW_windowFlags>(_config.flags)
    );
    CGFW_STABLE_THROW(rgfwWindow == nullptr, "failed to create the window {}", _config.title) {
        deinit_without_windows();
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

    return cvk::Surface{
        instance,
        cvk::Surface::State{.vkSurface = vkSurface, .extentCallback = [this] { return extent(); }},
    };
}

VkExtent2D Window::extent() const {
    debug_check(*this);

    i32 width = 0;
    i32 height = 0;
    RGFW_window_getSize(_handle.get(), &width, &height);
    return VkExtent2D{
        static_cast<uint32_t>(std::max(width, 0)),
        static_cast<uint32_t>(std::max(height, 0))
    };
}

void Window::pollEvents() {
    debug_check(*this);

    _events.clear();
    RGFW_pollEvents();

    auto const clock = dev::input_clock::Now();

    RGFW_event rgfwEvent{};
    while(RGFW_window_checkQueuedEvent(_handle.get(), &rgfwEvent) != RGFW_FALSE) {
        auto event = dev::to_event(rgfwEvent, clock, &RGFW_physicalToKeyChar);
        if(event)
            _events.push_back(std::move(*event));
    }
}

bool Window::shouldClose() const {
    debug_check(*this);

    return RGFW_window_shouldClose(_handle.get()) != RGFW_FALSE;
}

native_window Window::nativeWindow() const {
    debug_check(*this);

#ifdef CTH_PLATFORM_WINDOWS
    // RGFW has no getter, it gives the Win32 surface this HINSTANCE too
    return native_window{
        .display = GetModuleHandleW(nullptr),
        .window = RGFW_window_getHWND(_handle.get())
    };
#else
    return native_window{
        .display = RGFW_getDisplay_Wayland(),
        .window = RGFW_window_getWindow_Wayland(_handle.get())
    };
#endif
}

void Window::destroy(RGFW_window* rgfwWindow) {
    CTH_CRITICAL(rgfwWindow == nullptr, "rgfwWindow must not be nullptr") {}

    RGFW_window_close(rgfwWindow);
    deinit_without_windows();
}

void Window::forget() { _events.clear(); }

std::vector<std::string> instance_extensions() {
    size_t count = 0;
    char const** const extensions = RGFW_getRequiredInstanceExtensions_Vulkan(&count);

    return std::vector<std::string>{std::from_range, std::span{extensions, count}};
}
}
