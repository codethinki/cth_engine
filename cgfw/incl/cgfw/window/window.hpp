#pragma once
#include "cvk/surface/surface.hpp"
#include "cvk/utility/constants.hpp"
#include "cvk/utility/handle/unique_handle.hpp"
#include "cvk/utility/types.hpp"

#include <volk.h>

#include <cth/enums.hpp>
#include <cth/io/log.hpp>
#include <cth/ptr/not_null.hpp>

#include <cstdint>
#include <string>
#include <vector>


struct RGFW_window;

namespace cvk {
class Instance;
}

namespace cgfw {
using cvk::create;
using cvk::create_t;

/**
 * creation flags of a `Window`
 * @details the values of the `RGFW_windowFlags`
 */
enum class WindowFlags : uint32_t {
    NONE = 0,
    NO_BORDER = cth::en::flag_val(0),
    NO_RESIZE = cth::en::flag_val(1),
    FULLSCREEN = cth::en::flag_val(19), ///< borderless, the monitor keeps its video mode
    CENTER = cth::en::flag_val(6),
    SCALE_TO_MONITOR = cth::en::flag_val(8),
    /**
     * the window is not visible
     */
    HIDE = cth::en::flag_val(9),
    MAXIMIZE = cth::en::flag_val(10),
    FLOATING = cth::en::flag_val(12),
    FOCUS_ON_SHOW = cth::en::flag_val(13),
    MINIMIZE = cth::en::flag_val(14),
};
}

CTH_GEN_ENUM_FLAG_OVERLOADS(cgfw::WindowFlags)

namespace cgfw {
/**
 * opaque native handles of a window
 * @details windows: `display = HINSTANCE`, `window = HWND`
 * @details wayland: `display = wl_display*`, `window = wl_surface*`
 * @details windows that exist at the same time have the same display
 */
struct native_window {
    void* display;
    void* window;
};

namespace dev {
    /**
     * deleter of a window, without context
     */
    struct window_deleter {
        void operator()(RGFW_window*) const;
    };
}

/**
 * `RGFW_window` wrapper
 * @details a window of the platform: Win32 or Wayland
 * @details the first window initializes RGFW, the destruction of the last one deinitializes it
 * @details `pollEvents()` handles the events of every window
 * @attention not thread safe, also across windows
 */
class Window {
public:
    using handle_t = RGFW_window*;
    struct State;

    /**
     * title, size and flags of the window
     */
    struct Config {
        std::string title{};

        /**
         * requested size of the window
         * @pre `width > 0`, `height > 0`
         */
        VkExtent2D extent{};

        WindowFlags flags = WindowFlags::NONE;

        static constexpr void debug_check(Config const&);
    };

    /**
     * base constructor
     */
    explicit Window(Config);

    /**
     * constructs and wraps
     * @details calls @ref Window(Config), `wrap()`
     */
    Window(Config, State const&);

    /**
     * constructs and creates
     * @details calls @ref Window(Config), `create()`
     */
    explicit Window(Config, create_t);

    /**
     * @note calls `optDestroy()`
     */
    ~Window() { optDestroy(); }

    /**
     * wraps the state
     * @pre `RGFW_createWindow()` created the window with the config
     * @post `created()`
     * @note calls `optDestroy()`
     */
    void wrap(State const&);

    /**
     * creates the window with the config
     * @details RGFW is not initialized -> initializes RGFW with Vulkan first
     * @pre `Config::debug_check()` passes for the config
     * @post `created()`
     * @note calls `optDestroy()`
     * @throws cgfw::cgfw_exception reason: `RGFW_init()` failed, e.g. no display
     * @throws cgfw::cgfw_exception reason: `RGFW_createWindow()` failed
     */
    void create();

    /**
     * destroys and forgets
     * @details the last window deinitializes RGFW
     * @pre `created()`
     * @pre every surface of the window is destroyed
     */
    void destroy();

    /**
     * if `created()` -> calls `destroy()`
     */
    void optDestroy() {
        if(created())
            destroy();
    }

    /**
     * releases ownership, returns the state and forgets
     * @pre `created()`
     */
    State release();

    /**
     * creates a surface of the window
     * @details the extent callback of the surface reads `extent()`, see `cvk::Surface::State::extentCallback`
     * @pre `created()`
     * @pre the instance enables `instance_extensions()`
     * @pre the window outlives the surface and does not move
     * @throws cgfw::cgfw_exception reason: `RGFW_window_createSurface_Vulkan()` failed, the message has the `VkResult`
     */
    [[nodiscard]] cvk::Surface surface(cvk::Instance const&) const;

    //TODO review the size of RGFW: our fork keeps the buffer scale of Wayland at 1, the size is in buffer pixels
    //TODO the buffer scale of Wayland (HiDPI): the surface stays at scale 1, a scaled output upscales the window
    /**
     * @return current size of the framebuffer in pixels
     * @pre `created()`
     */
    [[nodiscard]] VkExtent2D extent() const;

    /**
     * handles the pending events of every window
     * @details returns at once, no wait for an event: for a render loop, it answers the pings of the compositor
     * @pre `created()`
     */
    void pollEvents() const;

    /**
     * @return the user or the platform requested to close the window
     * @details `pollEvents()` receives the requests
     * @pre `created()`
     */
    [[nodiscard]] bool shouldClose() const;

    /**
     * @return the native handles of the window
     * @pre `created()`
     */
    [[nodiscard]] native_window nativeWindow() const;

    /**
     * closes the window and frees it
     * @details the last window of RGFW deinitializes RGFW
     * @pre the window is not `nullptr`
     */
    static void destroy(RGFW_window*);

    [[nodiscard]] constexpr bool created() const { return static_cast<bool>(_handle); }
    [[nodiscard]] constexpr handle_t get() const { return _handle.get(); }
    [[nodiscard]] constexpr Config const& config() const { return _config; }

private:
    void forget();

    Config _config;
    cvk::unique_handle<handle_t, dev::window_deleter> _handle{};

public:
    Window(Window const& other) = delete;
    Window(Window&& other) noexcept = default;
    Window& operator=(Window const& other) = delete;
    Window& operator=(Window&& other) noexcept = default;

    static constexpr void debug_check(Window const&);
};

/**
 * @return the instance extensions of `Window::surface()`
 */
[[nodiscard]] std::vector<std::string> instance_extensions();
}

namespace cgfw {
struct Window::State {
    //TODO review stays a not_null pointer: the raw handle of RGFW, like vk_not_null for Vulkan handles
    cth::not_null_ptr<RGFW_window> rgfwWindow;
};
}

namespace cgfw {
constexpr void Window::Config::debug_check(Config const& config) {
    CTH_CRITICAL(
        config.extent.width == 0 || config.extent.height == 0,
        "the extent must not be 0, got {}x{}",
        config.extent.width,
        config.extent.height
    ) {}
}

constexpr void Window::debug_check(Window const& window) {
    CTH_CRITICAL(!window.created(), "window must be created") {}
}
}
