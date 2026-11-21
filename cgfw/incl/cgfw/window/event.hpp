#pragma once
#include <cth/chrono.hpp>
#include <cth/io/axis.hpp>
#include <cth/io/keybd/keys.hpp>

#include <string>
#include <variant>


namespace cgfw {
/**
 * text input of a window
 * @details one event per character, a dead key and the next key give one character
 */
struct TextEvent {
    std::string text; ///< UTF-8
    cth::chrono::time_point_t time;

    [[nodiscard]] constexpr bool operator==(TextEvent const&) const = default;
};

/**
 * the window gets or loses the keyboard focus
 */
struct FocusEvent {
    bool focused;
    cth::chrono::time_point_t time;

    [[nodiscard]] constexpr bool operator==(FocusEvent const&) const = default;
};

/**
 * input event of a window
 * @details
 * - keys and mouse buttons: `cth::io::key_update`
 * - mouse position and scroll: `cth::io::axis_update`
 */
using window_event = std::variant<cth::io::key_update, cth::io::axis_update, TextEvent, FocusEvent>;
}
