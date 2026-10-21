#pragma once
#include "cvk/surface/surface.hpp"
#include "cvk/utility/constants.hpp"
#include "cvk/utility/handle/unique_handle.hpp"
#include "cvk/utility/types.hpp"

#include <volk.h>

#include <cth/enums.hpp>
#include <cth/io/log.hpp>

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
 * @brief creation flags of a @ref Window, the values of the RGFW_windowFlags
 */
enum class WindowFlags : uint32_t {
    NONE = 0,
    NO_BORDER = cth::en::flag_val(0),
    NO_RESIZE = cth::en::flag_val(1),
    FULLSCREEN = cth::en::flag_val(4),
    CENTER = cth::en::flag_val(6),
    SCALE_TO_MONITOR = cth::en::flag_val(8),
    /**
     * @brief the window is not visible
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
namespace dev {
    /**
     * @brief deleter of a window, without context: RGFW keeps one global state
     * @details calls @ref Window::destroy(RGFW_window*)
     */
    struct window_deleter {
        void operator()(RGFW_window*) const;
    };
}

/**
 * @brief RGFW_window wrapper, a window of the platform: Win32 or Wayland
 * @details RGFW keeps one global state: the first window initializes it, the destruction of the last one
 *  deinitializes it, @ref pollEvents() handles the events of every window
 * @attention not thread safe: RGFW keeps one global state
 */
class Window {
public:
    using handle_t = RGFW_window*;
    struct State;

    /**
     * @brief title, size and flags of the window
     */
    struct Config {
        std::string title{};

        /**
         * @brief requested size of the window
         * @pre width > 0, height > 0
         */
        VkExtent2D extent{};

        WindowFlags flags = WindowFlags::NONE;

        static constexpr void debug_check(Config const&);
    };

    /**
     * @brief base constructor
     */
    explicit Window(Config);

    /**
     * @brief constructs and wraps
     * @details calls:
     * - @ref Window(Config)
     * - @ref wrap()
     */
    Window(Config, State const&);

    /**
     * @brief constructs and creates
     * @details calls:
     * - @ref Window(Config)
     * - @ref create()
     */
    explicit Window(Config, create_t);

    /**
     * @note calls @ref optDestroy()
     */
    ~Window() { optDestroy(); }

    /**
     * @brief wraps the state
     * @pre RGFW_createWindow() created the window with the config
     * @post @ref created()
     * @note calls @ref optDestroy()
     */
    void wrap(State const&);

    /**
     * @brief creates the window with the config
     * @details no window exists -> initializes RGFW first
     * @pre @ref Config::debug_check() passes for the config
     * @post @ref created()
     * @note calls @ref optDestroy()
     * @throws cgfw::cgfw_exception reason: RGFW_createWindow() failed, e.g. no display
     */
    void create();

    /**
     * @brief destroys and forgets
     * @pre @ref created()
     * @pre every surface of the window is destroyed
     * @note the last window deinitializes RGFW
     */
    void destroy();

    /**
     * @brief if @ref created() -> calls @ref destroy()
     */
    void optDestroy() { if(created()) destroy(); }

    /**
     * @brief releases ownership, returns the state and forgets
     * @pre @ref created()
     */
    State release();

    /**
     * @brief creates a surface of the window
     * @pre @ref created()
     * @pre the instance enables @ref instance_extensions()
     * @throws cgfw::cgfw_exception reason: RGFW_window_createSurface_Vulkan() failed, the message has the VkResult
     */
    [[nodiscard]] cvk::Surface surface(cvk::Instance const&) const;

    /**
     * @brief handles the pending events of every window
     * @details RGFW handles the events of all windows together
     * @pre @ref created()
     */
    void pollEvents() const;

    /**
     * @return the user or the platform requested to close the window
     * @details @ref pollEvents() receives the requests
     * @pre @ref created()
     */
    [[nodiscard]] bool shouldClose() const;

    /**
     * @brief closes the window and frees it
     * @pre the window is not nullptr
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
 * @return the instance extensions of @ref Window::surface(): VK_KHR_surface and the surface extension of the platform
 */
[[nodiscard]] std::vector<std::string> instance_extensions();
}

namespace cgfw {
struct Window::State {
    cvk::not_null_ptr<RGFW_window> rgfwWindow;
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
