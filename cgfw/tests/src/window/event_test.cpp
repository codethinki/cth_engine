#include "test.hpp"

#include "window/rgfw_event.hpp"

#include <RGFW.h>

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>


namespace cgfw::dev {
namespace {
    using cth::io::Axis;
    using cth::io::axis_update;
    using cth::io::ex_key;
    using cth::io::Key;
    using cth::io::key_update;
    using std::chrono::milliseconds;

    constexpr uint32_t OS_NOW = 100'000;

    /**
     * fixed clocks: the tests know the time points
     */
    [[nodiscard]] input_clock test_clock() {
        return input_clock{.now = cth::chrono::time_point_t{std::chrono::hours{1}}, .osNow = OS_NOW};
    }

    /**
     * `A` gives `a`, `Q` gives `€`, the other keys give none
     */
    [[nodiscard]] u32 test_key_char(RGFW_key key) {
        if(key == RGFW_keyA)
            return 'a';
        if(key == RGFW_keyQ)
            return U'\x20AC';
        return 0;
    }

    [[nodiscard]] std::optional<window_event> convert(RGFW_event const& rgfwEvent) {
        return to_event(rgfwEvent, test_clock(), &test_key_char);
    }

    [[nodiscard]] RGFW_event key_event(RGFW_eventType type, RGFW_key key, bool repeat, uint32_t time) {
        RGFW_event rgfwEvent{};
        rgfwEvent.key.type = type;
        rgfwEvent.key.value = key;
        rgfwEvent.key.repeat = repeat ? RGFW_TRUE : RGFW_FALSE;
        rgfwEvent.key.state = type == RGFW_keyPressed ? RGFW_TRUE : RGFW_FALSE;
        rgfwEvent.key.time = time;
        return rgfwEvent;
    }

    [[nodiscard]] RGFW_event button_event(RGFW_eventType type, RGFW_mouseButton button, uint32_t time) {
        RGFW_event rgfwEvent{};
        rgfwEvent.button.type = type;
        rgfwEvent.button.value = button;
        rgfwEvent.button.state = type == RGFW_mouseButtonPressed ? RGFW_TRUE : RGFW_FALSE;
        rgfwEvent.button.time = time;
        return rgfwEvent;
    }

    [[nodiscard]] RGFW_event type_event(RGFW_eventType type) {
        RGFW_event rgfwEvent{};
        rgfwEvent.common.type = type;
        return rgfwEvent;
    }
}

CGFW_TEST(to_ex_key, letters_and_digits) {
    static_assert(to_ex_key(RGFW_keyA) == ex_key{Key::A});
    static_assert(to_ex_key(RGFW_keyZ) == ex_key{Key::Z});
    static_assert(to_ex_key(RGFW_key0) == ex_key{Key::DIGIT_0});
    static_assert(to_ex_key(RGFW_key9) == ex_key{Key::DIGIT_9});
}

CGFW_TEST(to_ex_key, symbols_by_us_position) {
    static_assert(to_ex_key(RGFW_keyBacktick) == ex_key{Key::BACKTICK});
    static_assert(to_ex_key(RGFW_keyMinus) == ex_key{Key::MINUS});
    static_assert(to_ex_key(RGFW_keyEqual) == ex_key{Key::EQUAL});
    static_assert(to_ex_key(RGFW_keyBracket) == ex_key{Key::OPEN_BRACKET});
    static_assert(to_ex_key(RGFW_keyCloseBracket) == ex_key{Key::CLOSE_BRACKET});
    static_assert(to_ex_key(RGFW_keyBackSlash) == ex_key{Key::BACKSLASH});
    static_assert(to_ex_key(RGFW_keySemicolon) == ex_key{Key::SEMICOLON});
    static_assert(to_ex_key(RGFW_keyApostrophe) == ex_key{Key::QUOTE});
    static_assert(to_ex_key(RGFW_keyComma) == ex_key{Key::COMMA});
    static_assert(to_ex_key(RGFW_keyPeriod) == ex_key{Key::PERIOD});
    static_assert(to_ex_key(RGFW_keySlash) == ex_key{Key::SLASH});
    static_assert(to_ex_key(RGFW_keyWorld1) == ex_key{Key::LESS_THAN});
}

CGFW_TEST(to_ex_key, editing_and_navigation) {
    static_assert(to_ex_key(RGFW_keyEscape) == ex_key{Key::ESCAPE});
    static_assert(to_ex_key(RGFW_keyBackSpace) == ex_key{Key::BACKSPACE});
    static_assert(to_ex_key(RGFW_keyTab) == ex_key{Key::TAB});
    static_assert(to_ex_key(RGFW_keyReturn) == ex_key{Key::ENTER});
    static_assert(to_ex_key(RGFW_keySpace) == ex_key{Key::SPACE});
    static_assert(to_ex_key(RGFW_keyDelete) == ex_key{Key::DELETE});
    static_assert(to_ex_key(RGFW_keyInsert) == ex_key{Key::INSERT});
    static_assert(to_ex_key(RGFW_keyHome) == ex_key{Key::HOME});
    static_assert(to_ex_key(RGFW_keyEnd) == ex_key{Key::END});
    static_assert(to_ex_key(RGFW_keyPageUp) == ex_key{Key::PAGE_UP});
    static_assert(to_ex_key(RGFW_keyPageDown) == ex_key{Key::PAGE_DOWN});
    static_assert(to_ex_key(RGFW_keyUp) == ex_key{Key::ARROW_UP});
    static_assert(to_ex_key(RGFW_keyDown) == ex_key{Key::ARROW_DOWN});
    static_assert(to_ex_key(RGFW_keyLeft) == ex_key{Key::ARROW_LEFT});
    static_assert(to_ex_key(RGFW_keyRight) == ex_key{Key::ARROW_RIGHT});
}

CGFW_TEST(to_ex_key, function_and_system_keys) {
    static_assert(to_ex_key(RGFW_keyF1) == ex_key{Key::F1});
    static_assert(to_ex_key(RGFW_keyF12) == ex_key{Key::F12});
    static_assert(to_ex_key(RGFW_keyF13) == ex_key{Key::F13});
    static_assert(to_ex_key(RGFW_keyF24) == ex_key{Key::F24});
    static_assert(to_ex_key(RGFW_keyCapsLock) == ex_key{Key::CAPS_LOCK});
    static_assert(to_ex_key(RGFW_keyNumLock) == ex_key{Key::NUM_LOCK});
    static_assert(to_ex_key(RGFW_keyScrollLock) == ex_key{Key::SCROLL_LOCK});
    static_assert(to_ex_key(RGFW_keyPrintScreen) == ex_key{Key::PRINT_SCREEN});
    static_assert(to_ex_key(RGFW_keyPause) == ex_key{Key::PAUSE});
    static_assert(to_ex_key(RGFW_keyMenu) == ex_key{Key::MENU});
}

CGFW_TEST(to_ex_key, modifiers_set_right_for_the_right_copy) {
    static_assert(to_ex_key(RGFW_keyShiftL) == ex_key{Key::SHIFT});
    static_assert(to_ex_key(RGFW_keyShiftR) == ex_key{Key::SHIFT, true});
    static_assert(to_ex_key(RGFW_keyControlL) == ex_key{Key::CONTROL});
    static_assert(to_ex_key(RGFW_keyControlR) == ex_key{Key::CONTROL, true});
    static_assert(to_ex_key(RGFW_keyAltL) == ex_key{Key::ALT});
    static_assert(to_ex_key(RGFW_keyAltR) == ex_key{Key::ALT, true});
    static_assert(to_ex_key(RGFW_keySuperL) == ex_key{Key::PLATFORM});
    static_assert(to_ex_key(RGFW_keySuperR) == ex_key{Key::PLATFORM, true});
}

CGFW_TEST(to_ex_key, keypad_is_the_main_key_and_right) {
    static_assert(to_ex_key(RGFW_keyPad0) == ex_key{Key::DIGIT_0, true});
    static_assert(to_ex_key(RGFW_keyPad1) == ex_key{Key::DIGIT_1, true});
    static_assert(to_ex_key(RGFW_keyPad9) == ex_key{Key::DIGIT_9, true});
    static_assert(to_ex_key(RGFW_keyPadPeriod) == ex_key{Key::PERIOD, true});
    static_assert(to_ex_key(RGFW_keyPadSlash) == ex_key{Key::SLASH, true});
    static_assert(to_ex_key(RGFW_keyPadMultiply) == ex_key{Key::STAR, true});
    static_assert(to_ex_key(RGFW_keyPadMinus) == ex_key{Key::MINUS, true});
    static_assert(to_ex_key(RGFW_keyPadPlus) == ex_key{Key::PLUS, true});
    static_assert(to_ex_key(RGFW_keyPadEqual) == ex_key{Key::EQUAL, true});
    static_assert(to_ex_key(RGFW_keyPadReturn) == ex_key{Key::ENTER, true});
}

CGFW_TEST(to_ex_key, keys_without_a_cth_key) {
    static_assert(!to_ex_key(RGFW_keyNULL).has_value());
    static_assert(!to_ex_key(RGFW_keyF25).has_value());
    static_assert(!to_ex_key(RGFW_keyWorld2).has_value());
}

CGFW_TEST(to_mouse_key, buttons) {
    static_assert(to_mouse_key(RGFW_mouseLeft) == Key::MOUSE_LEFT);
    static_assert(to_mouse_key(RGFW_mouseRight) == Key::MOUSE_RIGHT);
    static_assert(to_mouse_key(RGFW_mouseMiddle) == Key::MOUSE_MIDDLE);
    static_assert(to_mouse_key(RGFW_mouseMisc1) == Key::MOUSE_4);
    static_assert(to_mouse_key(RGFW_mouseMisc2) == Key::MOUSE_5);
    static_assert(!to_mouse_key(RGFW_mouseMisc3).has_value());
}

CGFW_TEST(to_utf8, one_to_four_bytes) {
    static_assert(to_utf8(U'\x20AC') == "\xE2\x82\xAC");

    EXPECT_EQ(to_utf8(U'a'), "a");
    EXPECT_EQ(to_utf8(U'\r'), "\r");
    EXPECT_EQ(to_utf8(U'\x7F'), "\x7F");
    EXPECT_EQ(to_utf8(U'\xDF'), "\xC3\x9F");
    EXPECT_EQ(to_utf8(U'\x7FF'), "\xDF\xBF");
    EXPECT_EQ(to_utf8(U'\x20AC'), "\xE2\x82\xAC");
    EXPECT_EQ(to_utf8(U'\xFFFF'), "\xEF\xBF\xBF");
    EXPECT_EQ(to_utf8(U'\x1F600'), "\xF0\x9F\x98\x80");
    EXPECT_EQ(to_utf8(U'\x10FFFF'), "\xF4\x8F\xBF\xBF");
}

CGFW_TEST(to_utf8, no_scalar_value_is_empty) {
    EXPECT_EQ(to_utf8(0), "");
    EXPECT_EQ(to_utf8(char32_t{0xD800}), "");
    EXPECT_EQ(to_utf8(char32_t{0xDFFF}), "");
    EXPECT_EQ(to_utf8(char32_t{0x110000}), "");
}

CGFW_TEST(input_clock, time_point_is_now_minus_the_age) {
    auto const clock = test_clock();

    EXPECT_EQ(clock.timePoint(OS_NOW), clock.now);
    EXPECT_EQ(clock.timePoint(OS_NOW - 16), clock.now - milliseconds{16});
}

CGFW_TEST(input_clock, age_is_right_across_the_wrap) {
    constexpr input_clock clock{.now = cth::chrono::time_point_t{std::chrono::hours{1}}, .osNow = 5};
    static_assert(clock.timePoint(0xFFFF'FFF6) == clock.now - milliseconds{15});

    EXPECT_EQ(clock.timePoint(0xFFFF'FFF6), clock.now - milliseconds{15});
}

CGFW_TEST(input_clock, no_os_time_and_a_time_after_now_give_now) {
    auto const clock = test_clock();

    EXPECT_EQ(clock.timePoint(0), clock.now);
    EXPECT_EQ(clock.timePoint(OS_NOW + 10), clock.now);
}

CGFW_TEST(input_clock, now_reads_both_clocks) {
    auto const before = cth::chrono::clock_t::now();
    auto const clock = input_clock::Now();

    EXPECT_GE(clock.now, before);
    EXPECT_EQ(clock.timePoint(clock.osNow), clock.now);
}

CGFW_TEST(to_event, key_press) {
    auto const event = convert(key_event(RGFW_keyPressed, RGFW_keyA, false, OS_NOW - 5));

    ASSERT_TRUE(event.has_value());
    ASSERT_TRUE(std::holds_alternative<key_update>(*event));
    EXPECT_EQ(
        std::get<key_update>(*event),
        (key_update{.data = {ex_key{Key::A}, true, "a", false}, .time = test_clock().now - milliseconds{5}})
    );
}

CGFW_TEST(to_event, key_release_and_repeat) {
    auto const release = convert(key_event(RGFW_keyReleased, RGFW_keyQ, false, OS_NOW));
    auto const repeat = convert(key_event(RGFW_keyPressed, RGFW_keyQ, true, OS_NOW));

    ASSERT_TRUE(release.has_value() && repeat.has_value());
    EXPECT_EQ(
        std::get<key_update>(*release).data,
        (cth::io::key_state{ex_key{Key::Q}, false, "\xE2\x82\xAC", false})
    );
    EXPECT_EQ(
        std::get<key_update>(*repeat).data,
        (cth::io::key_state{ex_key{Key::Q}, true, "\xE2\x82\xAC", true})
    );
}

CGFW_TEST(to_event, keypad_key_without_character) {
    auto const event = convert(key_event(RGFW_keyPressed, RGFW_keyPadReturn, false, OS_NOW));

    ASSERT_TRUE(event.has_value());
    EXPECT_EQ(std::get<key_update>(*event).data, (cth::io::key_state{ex_key{Key::ENTER, true}, true}));
}

CGFW_TEST(to_event, mouse_button) {
    auto const press = convert(button_event(RGFW_mouseButtonPressed, RGFW_mouseMisc1, OS_NOW - 1));
    auto const release = convert(button_event(RGFW_mouseButtonReleased, RGFW_mouseLeft, OS_NOW));

    ASSERT_TRUE(press.has_value() && release.has_value());
    EXPECT_EQ(
        std::get<key_update>(*press),
        (key_update{.data = {Key::MOUSE_4, false, true}, .time = test_clock().now - milliseconds{1}})
    );
    EXPECT_EQ(std::get<key_update>(*release).data, (cth::io::key_state{Key::MOUSE_LEFT, false, false}));
}

CGFW_TEST(to_event, mouse_position) {
    RGFW_event rgfwEvent{};
    rgfwEvent.mouse.type = RGFW_mouseMotion;
    rgfwEvent.mouse.x = 12;
    rgfwEvent.mouse.y = -3;
    rgfwEvent.mouse.time = OS_NOW - 2;

    auto const event = convert(rgfwEvent);

    ASSERT_TRUE(event.has_value());
    EXPECT_EQ(
        std::get<axis_update>(*event),
        (axis_update{
            .data = {.axis = Axis::MOUSE_POSITION, .value = {12.f, -3.f}},
            .time = test_clock().now - milliseconds{2}
        })
    );
}

CGFW_TEST(to_event, mouse_scroll) {
    RGFW_event rgfwEvent{};
    rgfwEvent.delta.type = RGFW_mouseScroll;
    rgfwEvent.delta.x = 0.5f;
    rgfwEvent.delta.y = -1.f;
    rgfwEvent.delta.time = OS_NOW;

    auto const event = convert(rgfwEvent);

    ASSERT_TRUE(event.has_value());
    EXPECT_EQ(
        std::get<axis_update>(*event).data,
        (cth::io::axis_state{.axis = Axis::MOUSE_SCROLL, .value = {0.5f, -1.f}})
    );
}

CGFW_TEST(to_event, key_char_is_text) {
    RGFW_event rgfwEvent{};
    rgfwEvent.keyChar.type = RGFW_keyChar;
    rgfwEvent.keyChar.value = U'\x1F600';
    rgfwEvent.keyChar.time = OS_NOW - 3;

    auto const event = convert(rgfwEvent);

    ASSERT_TRUE(event.has_value());
    EXPECT_EQ(
        std::get<TextEvent>(*event),
        (TextEvent{.text = "\xF0\x9F\x98\x80", .time = test_clock().now - milliseconds{3}})
    );
}

CGFW_TEST(to_event, focus_takes_the_time_of_the_conversion) {
    auto const in = convert(type_event(RGFW_windowFocusIn));
    auto const out = convert(type_event(RGFW_windowFocusOut));

    ASSERT_TRUE(in.has_value() && out.has_value());
    EXPECT_EQ(std::get<FocusEvent>(*in), (FocusEvent{.focused = true, .time = test_clock().now}));
    EXPECT_EQ(std::get<FocusEvent>(*out), (FocusEvent{.focused = false, .time = test_clock().now}));
}

CGFW_TEST(to_event, skips_events_without_a_window_event) {
    EXPECT_FALSE(convert(key_event(RGFW_keyPressed, RGFW_keyWorld2, false, OS_NOW)).has_value());
    EXPECT_FALSE(convert(button_event(RGFW_mouseButtonPressed, RGFW_mouseMisc3, OS_NOW)).has_value());

    RGFW_event rawMotion{};
    rawMotion.delta.type = RGFW_mouseRawMotion;
    EXPECT_FALSE(convert(rawMotion).has_value());

    RGFW_event noCharacter{};
    noCharacter.keyChar.type = RGFW_keyChar;
    noCharacter.keyChar.value = 0xD800;
    EXPECT_FALSE(convert(noCharacter).has_value());

    EXPECT_FALSE(convert(type_event(RGFW_windowResized)).has_value());
    EXPECT_FALSE(convert(type_event(RGFW_windowClose)).has_value());
    EXPECT_FALSE(convert(type_event(RGFW_mouseEnter)).has_value());
}
}
