#pragma once
#include "cgfw/window/event.hpp"

#include <imgui.h>

#include <cth/io/keybd/keys.hpp>
#include <cth/io/log.hpp>

#include <array>
#include <cstddef>
#include <optional>


namespace timgui::dev {
/**
 * @return the ImGui key of the key state, `ImGuiKey_None` -> no ImGui key
 * @details
 * - not `cth::io::ex_key::right`, `cth::io::key_state::character` is one ASCII letter or digit
 *  -> the key of the character: the shortcuts follow the layout
 * - else the key at the position
 * - `cth::io::ex_key::right` -> the right modifier or the keypad key
 * - mouse key -> `ImGuiKey_None`, see `to_imgui_button()`
 */
[[nodiscard]] constexpr ImGuiKey to_imgui_key(cth::io::key_state const&);

/**
 * @return the modifier of the left or the right modifier key, `ImGuiMod_None` -> no modifier key
 */
[[nodiscard]] constexpr ImGuiKey to_imgui_mod(ImGuiKey);

/**
 * @return the ImGui mouse button of the mouse key, `std::nullopt` -> no mouse key
 * @details left `0`, right `1`, middle `2`, `MOUSE_4` `3`, `MOUSE_5` `4`
 */
[[nodiscard]] constexpr std::optional<ImGuiMouseButton> to_imgui_button(cth::io::Key);

/**
 * the modifier keys that are down
 * @details a modifier is down while its left or its right key is down
 */
class modifier_keys {
public:
    /**
     * sets the state of the modifier key
     * @return the modifier of the key is down
     * @pre `to_imgui_mod(key) != ImGuiMod_None`
     */
    constexpr bool set(ImGuiKey key, bool down);

private:
    // the left keys, then the right keys
    static constexpr size_t SIDE_KEYS = ImGuiKey_RightCtrl - ImGuiKey_LeftCtrl;

    std::array<bool, 2 * SIDE_KEYS> _down{}; // from `ImGuiKey_LeftCtrl` to `ImGuiKey_RightSuper`
};

/**
 * passes the window event to ImGui
 * @details
 * - key -> `ImGuiIO::AddKeyEvent()` of `to_imgui_key()`
 * - modifier key -> its `ImGuiMod_*` first
 * - repeat -> passes `down`, ImGui repeats itself
 * - mouse key -> `ImGuiIO::AddMouseButtonEvent()` of `to_imgui_button()`
 * - `cth::io::Axis::MOUSE_POSITION` -> `ImGuiIO::AddMousePosEvent()`
 * - `cth::io::Axis::MOUSE_SCROLL` -> `ImGuiIO::AddMouseWheelEvent()`
 * - scroll: positive y up, positive x left, like ImGui
 * - `cgfw::TextEvent` -> `ImGuiIO::AddInputCharactersUTF8()`
 * - `cgfw::FocusEvent` -> `ImGuiIO::AddFocusEvent()`
 * - focus loss -> forgets the modifier keys, like ImGui forgets its keys
 * @param[out] io gets the events
 * @param[out] modifiers the modifier keys that are down
 */
void add_event(ImGuiIO& io, cgfw::window_event const& event, modifier_keys& modifiers);
}

namespace timgui::dev {
constexpr ImGuiKey to_imgui_key(cth::io::key_state const& state) {
    using cth::io::Key;

    auto const key = state.exKey.key;
    auto const offset = [key](Key first, ImGuiKey to) {
        return static_cast<ImGuiKey>(to + (*key - *first));
    };

    if(state.exKey.right) {
        if(key >= Key::DIGIT_0 && key <= Key::DIGIT_9)
            return offset(Key::DIGIT_0, ImGuiKey_Keypad0);

        switch(key) {
            case Key::PERIOD: return ImGuiKey_KeypadDecimal;
            case Key::SLASH: return ImGuiKey_KeypadDivide;
            case Key::STAR: return ImGuiKey_KeypadMultiply;
            case Key::MINUS: return ImGuiKey_KeypadSubtract;
            case Key::PLUS: return ImGuiKey_KeypadAdd;
            case Key::ENTER: return ImGuiKey_KeypadEnter;
            case Key::EQUAL: return ImGuiKey_KeypadEqual;

            case Key::CONTROL: return ImGuiKey_RightCtrl;
            case Key::SHIFT: return ImGuiKey_RightShift;
            case Key::ALT: return ImGuiKey_RightAlt;
            case Key::PLATFORM: return ImGuiKey_RightSuper;

            default: break;
        }
    }
    else if(state.character.size() == 1) {
        auto const character = state.character[0];
        if(character >= 'a' && character <= 'z')
            return static_cast<ImGuiKey>(ImGuiKey_A + (character - 'a'));
        if(character >= 'A' && character <= 'Z')
            return static_cast<ImGuiKey>(ImGuiKey_A + (character - 'A'));
        if(character >= '0' && character <= '9')
            return static_cast<ImGuiKey>(ImGuiKey_0 + (character - '0'));
    }

    if(key >= Key::A && key <= Key::Z)
        return offset(Key::A, ImGuiKey_A);
    if(key >= Key::DIGIT_0 && key <= Key::DIGIT_9)
        return offset(Key::DIGIT_0, ImGuiKey_0);
    if(key >= Key::F1 && key <= Key::F12)
        return offset(Key::F1, ImGuiKey_F1);
    if(key >= Key::F13 && key <= Key::F24)
        return offset(Key::F13, ImGuiKey_F13);

    switch(key) {
        case Key::SPACE: return ImGuiKey_Space;
        case Key::BACKTICK: return ImGuiKey_GraveAccent;
        case Key::MINUS: return ImGuiKey_Minus;
        case Key::EQUAL: return ImGuiKey_Equal;
        case Key::OPEN_BRACKET: return ImGuiKey_LeftBracket;
        case Key::CLOSE_BRACKET: return ImGuiKey_RightBracket;
        case Key::BACKSLASH: return ImGuiKey_Backslash;
        case Key::SEMICOLON: return ImGuiKey_Semicolon;
        case Key::QUOTE: return ImGuiKey_Apostrophe;
        case Key::COMMA: return ImGuiKey_Comma;
        case Key::PERIOD: return ImGuiKey_Period;
        case Key::SLASH: return ImGuiKey_Slash;
        case Key::LESS_THAN: return ImGuiKey_Oem102;

        case Key::BACKSPACE: return ImGuiKey_Backspace;
        case Key::CAPS_LOCK: return ImGuiKey_CapsLock;
        case Key::DEL: return ImGuiKey_Delete;
        case Key::END: return ImGuiKey_End;
        case Key::ENTER: return ImGuiKey_Enter;
        case Key::ESCAPE: return ImGuiKey_Escape;
        case Key::HOME: return ImGuiKey_Home;
        case Key::INSERT: return ImGuiKey_Insert;
        case Key::PAGE_DOWN: return ImGuiKey_PageDown;
        case Key::PAGE_UP: return ImGuiKey_PageUp;
        case Key::TAB: return ImGuiKey_Tab;

        case Key::ARROW_DOWN: return ImGuiKey_DownArrow;
        case Key::ARROW_LEFT: return ImGuiKey_LeftArrow;
        case Key::ARROW_RIGHT: return ImGuiKey_RightArrow;
        case Key::ARROW_UP: return ImGuiKey_UpArrow;

        case Key::CONTROL: return ImGuiKey_LeftCtrl;
        case Key::SHIFT: return ImGuiKey_LeftShift;
        case Key::ALT: return ImGuiKey_LeftAlt;
        case Key::PLATFORM: return ImGuiKey_LeftSuper;

        case Key::NUM_LOCK: return ImGuiKey_NumLock;
        case Key::PRINT_SCREEN: return ImGuiKey_PrintScreen;
        case Key::SCROLL_LOCK: return ImGuiKey_ScrollLock;
        case Key::PAUSE: return ImGuiKey_Pause;
        case Key::MENU: return ImGuiKey_Menu;

        default: return ImGuiKey_None;
    }
}

constexpr ImGuiKey to_imgui_mod(ImGuiKey key) {
    switch(key) {
        case ImGuiKey_LeftCtrl:
        case ImGuiKey_RightCtrl: return ImGuiMod_Ctrl;
        case ImGuiKey_LeftShift:
        case ImGuiKey_RightShift: return ImGuiMod_Shift;
        case ImGuiKey_LeftAlt:
        case ImGuiKey_RightAlt: return ImGuiMod_Alt;
        case ImGuiKey_LeftSuper:
        case ImGuiKey_RightSuper: return ImGuiMod_Super;
        default: return ImGuiMod_None;
    }
}

constexpr std::optional<ImGuiMouseButton> to_imgui_button(cth::io::Key key) {
    using cth::io::Key;

    switch(key) {
        case Key::MOUSE_LEFT: return ImGuiMouseButton_Left;
        case Key::MOUSE_RIGHT: return ImGuiMouseButton_Right;
        case Key::MOUSE_MIDDLE: return ImGuiMouseButton_Middle;
        case Key::MOUSE_4: return 3;
        case Key::MOUSE_5: return 4;
        default: return std::nullopt;
    }
}

constexpr bool modifier_keys::set(ImGuiKey key, bool down) {
    CTH_CRITICAL(
        to_imgui_mod(key) == ImGuiMod_None,
        "the key must be a modifier key, got {}",
        static_cast<int>(key)
    ) {}

    auto const keyIdx = static_cast<size_t>(key - ImGuiKey_LeftCtrl);
    _down[keyIdx] = down;

    auto const leftIdx = keyIdx % SIDE_KEYS;
    return _down[leftIdx] || _down[leftIdx + SIDE_KEYS];
}
}
