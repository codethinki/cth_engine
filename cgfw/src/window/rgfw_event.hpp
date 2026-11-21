#pragma once
#include "cgfw/window/event.hpp"

#include <RGFW.h>

#include <cth/chrono.hpp>
#include <cth/io/keybd/keys.hpp>

#include <chrono>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>


namespace cgfw::dev {
/**
 * @return current time of the OS clock of the input events, in milliseconds
 * @details
 * - Linux: `CLOCK_MONOTONIC`, assumes the Wayland compositor takes its input times from it
 * - Windows: `GetTickCount()`, the clock of `GetMessageTime()`
 */
[[nodiscard]] uint32_t os_input_time();

/**
 * `cth::chrono::clock_t` and the OS clock of the input events at one moment
 */
struct input_clock {
    cth::chrono::time_point_t now;
    uint32_t osNow; ///< OS clock at `now`, in milliseconds, see `os_input_time()`

    /**
     * @return the clocks now
     */
    [[nodiscard]] static input_clock Now() {
        return {cth::chrono::clock_t::now(), os_input_time()};
    }

    /**
     * converts the OS time of an input event
     * @param osTime in milliseconds, `0` -> no OS time
     * @return `now - (osNow - osTime) mod 2^32`
     * @details
     * - `osTime == 0` -> `now`
     * - `osTime` after `osNow` (another clock) -> `now`
     */
    [[nodiscard]] constexpr cth::chrono::time_point_t timePoint(uint32_t osTime) const;
};

/**
 * @return UTF-8 of the codepoint, `0` or no Unicode scalar value -> empty
 */
[[nodiscard]] constexpr std::string to_utf8(char32_t codepoint);

/**
 * @return the key at the position of the RGFW key, `std::nullopt` -> no `cth::io::Key` for it
 * @details keypad keys and right modifiers set `cth::io::ex_key::right`
 */
[[nodiscard]] constexpr std::optional<cth::io::ex_key> to_ex_key(RGFW_key);

/**
 * @return the key of the mouse button, `std::nullopt` -> no `cth::io::Key` for it
 */
[[nodiscard]] constexpr std::optional<cth::io::Key> to_mouse_key(RGFW_mouseButton);

/**
 * converts an RGFW event
 * @param keyChar codepoint of a physical key in the layout, see `RGFW_physicalToKeyChar()`
 * @return `std::nullopt` -> no `window_event` for it
 * @details
 * - key -> `cth::io::key_update`, `cth::io::key_state::character` from `keyChar`
 * - mouse button -> `cth::io::key_update` of a mouse key
 * - mouse position, scroll -> `cth::io::axis_update`
 * - key character -> `TextEvent`, focus -> `FocusEvent`
 * - time: `input_clock::timePoint()` of the OS time, focus -> `input_clock::now`
 */
[[nodiscard]] std::optional<window_event> to_event(
    RGFW_event const& rgfwEvent,
    input_clock const& clock,
    u32 (*keyChar)(RGFW_key)
);
}

namespace cgfw::dev {
constexpr cth::chrono::time_point_t input_clock::timePoint(uint32_t osTime) const {
    if(osTime == 0)
        return now;

    // unsigned: the age is right across the wrap of the OS clock
    auto const age = static_cast<uint32_t>(osNow - osTime);
    if(age > static_cast<uint32_t>(std::numeric_limits<int32_t>::max()))
        return now;

    return now - std::chrono::milliseconds{age};
}

constexpr std::string to_utf8(char32_t codepoint) {
    auto const value = static_cast<uint32_t>(codepoint);
    if(value == 0 || value > 0x10FFFF || (value >= 0xD800 && value <= 0xDFFF))
        return {};

    auto const byte = [](uint32_t bits) {
        return static_cast<char>(static_cast<unsigned char>(bits));
    };

    if(value < 0x80)
        return std::string{byte(value)};
    if(value < 0x800)
        return std::string{byte(0xC0 | (value >> 6)), byte(0x80 | (value & 0x3F))};
    if(value < 0x10000)
        return std::string{
            byte(0xE0 | (value >> 12)),
            byte(0x80 | ((value >> 6) & 0x3F)),
            byte(0x80 | (value & 0x3F))
        };
    return std::string{
        byte(0xF0 | (value >> 18)),
        byte(0x80 | ((value >> 12) & 0x3F)),
        byte(0x80 | ((value >> 6) & 0x3F)),
        byte(0x80 | (value & 0x3F))
    };
}

constexpr std::optional<cth::io::ex_key> to_ex_key(RGFW_key key) {
    using cth::io::Key;
    using cth::io::ex_key;

    auto const offset = [](RGFW_key from, RGFW_key first, Key to) {
        return static_cast<Key>(*to + (from - first));
    };

    if(key >= RGFW_keyA && key <= RGFW_keyZ)
        return ex_key{offset(key, RGFW_keyA, Key::A)};
    if(key >= RGFW_key0 && key <= RGFW_key9)
        return ex_key{offset(key, RGFW_key0, Key::DIGIT_0)};
    if(key >= RGFW_keyF1 && key <= RGFW_keyF12)
        return ex_key{offset(key, RGFW_keyF1, Key::F1)};
    if(key >= RGFW_keyF13 && key <= RGFW_keyF24)
        return ex_key{offset(key, RGFW_keyF13, Key::F13)};
    if(key >= RGFW_keyPad1 && key <= RGFW_keyPad9)
        return ex_key{offset(key, RGFW_keyPad1, Key::DIGIT_1), true};

    switch(key) {
        case RGFW_keyEscape: return ex_key{Key::ESCAPE};
        case RGFW_keyBacktick: return ex_key{Key::BACKTICK};
        case RGFW_keyMinus: return ex_key{Key::MINUS};
        case RGFW_keyEqual: return ex_key{Key::EQUAL};
        case RGFW_keyBackSpace: return ex_key{Key::BACKSPACE};
        case RGFW_keyTab: return ex_key{Key::TAB};
        case RGFW_keySpace: return ex_key{Key::SPACE};
        case RGFW_keyPeriod: return ex_key{Key::PERIOD};
        case RGFW_keyComma: return ex_key{Key::COMMA};
        case RGFW_keySlash: return ex_key{Key::SLASH};
        case RGFW_keyBracket: return ex_key{Key::OPEN_BRACKET};
        case RGFW_keyCloseBracket: return ex_key{Key::CLOSE_BRACKET};
        case RGFW_keySemicolon: return ex_key{Key::SEMICOLON};
        case RGFW_keyApostrophe: return ex_key{Key::QUOTE};
        case RGFW_keyBackSlash: return ex_key{Key::BACKSLASH};
        case RGFW_keyReturn: return ex_key{Key::ENTER};
// windows.h defines DELETE
#pragma push_macro("DELETE")
#undef DELETE
        case RGFW_keyDelete: return ex_key{Key::DELETE};
#pragma pop_macro("DELETE")
        case RGFW_keyWorld1: return ex_key{Key::LESS_THAN};

        case RGFW_keyCapsLock: return ex_key{Key::CAPS_LOCK};
        case RGFW_keyShiftL: return ex_key{Key::SHIFT};
        case RGFW_keyShiftR: return ex_key{Key::SHIFT, true};
        case RGFW_keyControlL: return ex_key{Key::CONTROL};
        case RGFW_keyControlR: return ex_key{Key::CONTROL, true};
        case RGFW_keyAltL: return ex_key{Key::ALT};
        case RGFW_keyAltR: return ex_key{Key::ALT, true};
        case RGFW_keySuperL: return ex_key{Key::PLATFORM};
        case RGFW_keySuperR: return ex_key{Key::PLATFORM, true};
        case RGFW_keyMenu: return ex_key{Key::MENU};

        case RGFW_keyUp: return ex_key{Key::ARROW_UP};
        case RGFW_keyDown: return ex_key{Key::ARROW_DOWN};
        case RGFW_keyLeft: return ex_key{Key::ARROW_LEFT};
        case RGFW_keyRight: return ex_key{Key::ARROW_RIGHT};
        case RGFW_keyInsert: return ex_key{Key::INSERT};
        case RGFW_keyEnd: return ex_key{Key::END};
        case RGFW_keyHome: return ex_key{Key::HOME};
        case RGFW_keyPageUp: return ex_key{Key::PAGE_UP};
        case RGFW_keyPageDown: return ex_key{Key::PAGE_DOWN};

        case RGFW_keyNumLock: return ex_key{Key::NUM_LOCK};
        case RGFW_keyPadSlash: return ex_key{Key::SLASH, true};
        case RGFW_keyPadMultiply: return ex_key{Key::STAR, true};
        case RGFW_keyPadPlus: return ex_key{Key::PLUS, true};
        case RGFW_keyPadMinus: return ex_key{Key::MINUS, true};
        case RGFW_keyPadEqual: return ex_key{Key::EQUAL, true};
        case RGFW_keyPad0: return ex_key{Key::DIGIT_0, true};
        case RGFW_keyPadPeriod: return ex_key{Key::PERIOD, true};
        case RGFW_keyPadReturn: return ex_key{Key::ENTER, true};

        case RGFW_keyScrollLock: return ex_key{Key::SCROLL_LOCK};
        case RGFW_keyPrintScreen: return ex_key{Key::PRINT_SCREEN};
        case RGFW_keyPause: return ex_key{Key::PAUSE};

        default: return std::nullopt;
    }
}

constexpr std::optional<cth::io::Key> to_mouse_key(RGFW_mouseButton button) {
    using cth::io::Key;

    switch(button) {
        case RGFW_mouseLeft: return Key::MOUSE_LEFT;
        case RGFW_mouseRight: return Key::MOUSE_RIGHT;
        case RGFW_mouseMiddle: return Key::MOUSE_MIDDLE;
        case RGFW_mouseMisc1: return Key::MOUSE_4;
        case RGFW_mouseMisc2: return Key::MOUSE_5;
        default: return std::nullopt;
    }
}
}
