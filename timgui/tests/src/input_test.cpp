#include "test.hpp"

#include "input.hpp"

#include "cgfw/window/event.hpp"

#include <imgui.h>
#include <imgui_internal.h>

#include <cth/chrono.hpp>
#include <cth/io/axis.hpp>
#include <cth/io/keybd/keys.hpp>

#include <string>
#include <utility>
#include <vector>


namespace timgui::dev {
namespace {
    using cth::io::Axis;
    using cth::io::Key;
    using cth::io::key_state;

    /**
     * own ImGui context for the scope, current while it lives
     */
    class ScopedContext {
    public:
        ScopedContext() : _context{ImGui::CreateContext()} { ImGui::SetCurrentContext(_context); }
        ~ScopedContext() { ImGui::DestroyContext(_context); }

        /**
         * @return the events that the io queued, in order
         */
        [[nodiscard]] ImVector<ImGuiInputEvent> const& events() const {
            return _context->InputEventsQueue;
        }

    private:
        ImGuiContext* _context;

    public:
        ScopedContext(ScopedContext const& other) = delete;
        ScopedContext(ScopedContext&& other) = delete;
        ScopedContext& operator=(ScopedContext const& other) = delete;
        ScopedContext& operator=(ScopedContext&& other) = delete;
    };

    [[nodiscard]] cgfw::window_event key_event(
        Key key,
        bool right,
        bool down,
        std::string character = {},
        bool repeat = false
    ) {
        return cth::io::key_update{
            .data = key_state{key, right, down, std::move(character), repeat},
            .time = cth::chrono::clock_t::now()
        };
    }

    [[nodiscard]] cgfw::window_event axis_event(Axis axis, float x, float y) {
        return cth::io::axis_update{
            .data = {.axis = axis, .value = {x, y}},
            .time = cth::chrono::clock_t::now()
        };
    }

    /** the key and down of each key event */
    using key_events_t = std::vector<std::pair<ImGuiKey, bool>>;

    /**
     * @return the key events of the queue, in order
     */
    [[nodiscard]] key_events_t key_events(ScopedContext const& context) {
        key_events_t keys{};
        for(auto const& event : context.events())
            if(event.Type == ImGuiInputEventType_Key)
                keys.emplace_back(event.Key.Key, event.Key.Down);
        return keys;
    }
}

TIMGUI_TEST(to_imgui_key, letters_follow_the_layout) {
    static_assert(to_imgui_key(key_state{Key::Q, false, true, "a"}) == ImGuiKey_A);
    static_assert(to_imgui_key(key_state{Key::Y, false, true, "z"}) == ImGuiKey_Z);
    static_assert(to_imgui_key(key_state{Key::A, false, true, "A"}) == ImGuiKey_A);
    // AZERTY: m on the key right of L
    static_assert(to_imgui_key(key_state{Key::SEMICOLON, false, true, "m"}) == ImGuiKey_M);
}

TIMGUI_TEST(to_imgui_key, digits_follow_the_layout) {
    static_assert(to_imgui_key(key_state{Key::DIGIT_1, false, true, "1"}) == ImGuiKey_1);
    static_assert(to_imgui_key(key_state{Key::Q, false, true, "7"}) == ImGuiKey_7);
}

TIMGUI_TEST(to_imgui_key, other_characters_map_by_position) {
    // AZERTY: & on the key of 1
    static_assert(to_imgui_key(key_state{Key::DIGIT_1, false, true, "&"}) == ImGuiKey_1);
    // German: ö right of L, not ASCII
    static_assert(
        to_imgui_key(key_state{Key::SEMICOLON, false, true, "\xC3\xB6"}) == ImGuiKey_Semicolon
    );
    static_assert(to_imgui_key(key_state{Key::Q, false, true}) == ImGuiKey_Q);
    static_assert(to_imgui_key(key_state{Key::ENTER, false, true, "\r"}) == ImGuiKey_Enter);
    static_assert(to_imgui_key(key_state{Key::SPACE, false, true, " "}) == ImGuiKey_Space);
    static_assert(to_imgui_key(key_state{Key::LESS_THAN, false, true, "<"}) == ImGuiKey_Oem102);
}

TIMGUI_TEST(to_imgui_key, function_keys) {
    static_assert(to_imgui_key(key_state{Key::F1, false, true}) == ImGuiKey_F1);
    static_assert(to_imgui_key(key_state{Key::F12, false, true}) == ImGuiKey_F12);
    static_assert(to_imgui_key(key_state{Key::F13, false, true}) == ImGuiKey_F13);
    static_assert(to_imgui_key(key_state{Key::F24, false, true}) == ImGuiKey_F24);
}

TIMGUI_TEST(to_imgui_key, right_is_the_keypad) {
    static_assert(to_imgui_key(key_state{Key::DIGIT_0, true, true}) == ImGuiKey_Keypad0);
    static_assert(to_imgui_key(key_state{Key::DIGIT_9, true, true}) == ImGuiKey_Keypad9);
    // the character does not move a keypad key
    static_assert(to_imgui_key(key_state{Key::DIGIT_5, true, true, "5"}) == ImGuiKey_Keypad5);
    static_assert(to_imgui_key(key_state{Key::ENTER, true, true, "\r"}) == ImGuiKey_KeypadEnter);
    static_assert(to_imgui_key(key_state{Key::PERIOD, true, true}) == ImGuiKey_KeypadDecimal);
    static_assert(to_imgui_key(key_state{Key::SLASH, true, true}) == ImGuiKey_KeypadDivide);
    static_assert(to_imgui_key(key_state{Key::STAR, true, true}) == ImGuiKey_KeypadMultiply);
    static_assert(to_imgui_key(key_state{Key::MINUS, true, true}) == ImGuiKey_KeypadSubtract);
    static_assert(to_imgui_key(key_state{Key::PLUS, true, true}) == ImGuiKey_KeypadAdd);
    static_assert(to_imgui_key(key_state{Key::EQUAL, true, true}) == ImGuiKey_KeypadEqual);
}

TIMGUI_TEST(to_imgui_key, modifiers_by_side) {
    static_assert(to_imgui_key(key_state{Key::CONTROL, false, true}) == ImGuiKey_LeftCtrl);
    static_assert(to_imgui_key(key_state{Key::CONTROL, true, true}) == ImGuiKey_RightCtrl);
    static_assert(to_imgui_key(key_state{Key::SHIFT, false, true}) == ImGuiKey_LeftShift);
    static_assert(to_imgui_key(key_state{Key::SHIFT, true, true}) == ImGuiKey_RightShift);
    static_assert(to_imgui_key(key_state{Key::ALT, false, true}) == ImGuiKey_LeftAlt);
    static_assert(to_imgui_key(key_state{Key::ALT, true, true}) == ImGuiKey_RightAlt);
    static_assert(to_imgui_key(key_state{Key::PLATFORM, false, true}) == ImGuiKey_LeftSuper);
    static_assert(to_imgui_key(key_state{Key::PLATFORM, true, true}) == ImGuiKey_RightSuper);
}

TIMGUI_TEST(to_imgui_key, no_imgui_key) {
    static_assert(to_imgui_key(key_state{Key::NONE, false, true}) == ImGuiKey_None);
    static_assert(to_imgui_key(key_state{Key::META, false, true}) == ImGuiKey_None);
    static_assert(to_imgui_key(key_state{Key::MOUSE_LEFT, false, true}) == ImGuiKey_None);
    static_assert(to_imgui_key(key_state{Key::CUSTOM_0, false, true}) == ImGuiKey_None);
}

TIMGUI_TEST(to_imgui_mod, both_sides) {
    static_assert(to_imgui_mod(ImGuiKey_LeftCtrl) == ImGuiMod_Ctrl);
    static_assert(to_imgui_mod(ImGuiKey_RightCtrl) == ImGuiMod_Ctrl);
    static_assert(to_imgui_mod(ImGuiKey_LeftShift) == ImGuiMod_Shift);
    static_assert(to_imgui_mod(ImGuiKey_RightShift) == ImGuiMod_Shift);
    static_assert(to_imgui_mod(ImGuiKey_LeftAlt) == ImGuiMod_Alt);
    static_assert(to_imgui_mod(ImGuiKey_RightAlt) == ImGuiMod_Alt);
    static_assert(to_imgui_mod(ImGuiKey_LeftSuper) == ImGuiMod_Super);
    static_assert(to_imgui_mod(ImGuiKey_RightSuper) == ImGuiMod_Super);
    static_assert(to_imgui_mod(ImGuiKey_A) == ImGuiMod_None);
}

TIMGUI_TEST(to_imgui_button, mouse_keys) {
    static_assert(to_imgui_button(Key::MOUSE_LEFT) == 0);
    static_assert(to_imgui_button(Key::MOUSE_RIGHT) == 1);
    static_assert(to_imgui_button(Key::MOUSE_MIDDLE) == 2);
    static_assert(to_imgui_button(Key::MOUSE_4) == 3);
    static_assert(to_imgui_button(Key::MOUSE_5) == 4);
    static_assert(!to_imgui_button(Key::A).has_value());
}

TIMGUI_TEST(modifier_keys, down_while_one_side_is_down) {
    modifier_keys modifiers{};

    EXPECT_TRUE(modifiers.set(ImGuiKey_LeftCtrl, true));
    EXPECT_TRUE(modifiers.set(ImGuiKey_RightCtrl, true));
    EXPECT_TRUE(modifiers.set(ImGuiKey_LeftCtrl, false));
    EXPECT_FALSE(modifiers.set(ImGuiKey_RightCtrl, false));
}

TIMGUI_TEST(modifier_keys, modifiers_are_apart) {
    modifier_keys modifiers{};

    EXPECT_TRUE(modifiers.set(ImGuiKey_LeftShift, true));
    EXPECT_FALSE(modifiers.set(ImGuiKey_RightAlt, false));
    EXPECT_TRUE(modifiers.set(ImGuiKey_RightSuper, true));
    EXPECT_FALSE(modifiers.set(ImGuiKey_LeftShift, false));
}

TIMGUI_TEST(add_event, key_follows_the_layout) {
    ScopedContext const context{};
    modifier_keys modifiers{};

    add_event(ImGui::GetIO(), key_event(Key::Q, false, true, "a"), modifiers);
    add_event(ImGui::GetIO(), key_event(Key::Q, false, false, "a"), modifiers);

    EXPECT_EQ(key_events(context), (key_events_t{{ImGuiKey_A, true}, {ImGuiKey_A, false}}));
}

TIMGUI_TEST(add_event, modifier_event_before_the_key) {
    ScopedContext const context{};
    modifier_keys modifiers{};
    auto& io = ImGui::GetIO();

    add_event(io, key_event(Key::CONTROL, false, true), modifiers);
    add_event(io, key_event(Key::CONTROL, true, true), modifiers);
    add_event(io, key_event(Key::CONTROL, false, false), modifiers);
    add_event(io, key_event(Key::CONTROL, true, false), modifiers);

    // ImGui drops an event without a change: the second down and the first up of ImGuiMod_Ctrl
    EXPECT_EQ(
        key_events(context),
        (key_events_t{
            {ImGuiMod_Ctrl, true},
            {ImGuiKey_LeftCtrl, true},
            {ImGuiKey_RightCtrl, true},
            {ImGuiKey_LeftCtrl, false},
            {ImGuiMod_Ctrl, false},
            {ImGuiKey_RightCtrl, false},
        })
    );
}

TIMGUI_TEST(add_event, repeat_keeps_the_key_down) {
    ScopedContext const context{};
    modifier_keys modifiers{};
    auto& io = ImGui::GetIO();

    add_event(io, key_event(Key::DEL, false, true, "\x7F"), modifiers);
    add_event(io, key_event(Key::DEL, false, true, "\x7F", true), modifiers);

    EXPECT_EQ(key_events(context), (key_events_t{{ImGuiKey_Delete, true}}));
}

TIMGUI_TEST(add_event, mouse_button) {
    ScopedContext const context{};
    modifier_keys modifiers{};

    add_event(ImGui::GetIO(), key_event(Key::MOUSE_4, false, true), modifiers);

    ASSERT_EQ(context.events().Size, 1);
    auto const& event = context.events()[0];
    EXPECT_EQ(event.Type, ImGuiInputEventType_MouseButton);
    EXPECT_EQ(event.MouseButton.Button, 3);
    EXPECT_TRUE(event.MouseButton.Down);
}

TIMGUI_TEST(add_event, mouse_position) {
    ScopedContext const context{};
    modifier_keys modifiers{};

    add_event(ImGui::GetIO(), axis_event(Axis::MOUSE_POSITION, 10.f, 20.f), modifiers);

    ASSERT_EQ(context.events().Size, 1);
    auto const& event = context.events()[0];
    EXPECT_EQ(event.Type, ImGuiInputEventType_MousePos);
    EXPECT_EQ(event.MousePos.PosX, 10.f);
    EXPECT_EQ(event.MousePos.PosY, 20.f);
}

TIMGUI_TEST(add_event, scroll_keeps_the_sign) {
    ScopedContext const context{};
    modifier_keys modifiers{};

    add_event(ImGui::GetIO(), axis_event(Axis::MOUSE_SCROLL, 0.5f, -1.f), modifiers);

    ASSERT_EQ(context.events().Size, 1);
    auto const& event = context.events()[0];
    EXPECT_EQ(event.Type, ImGuiInputEventType_MouseWheel);
    EXPECT_EQ(event.MouseWheel.WheelX, 0.5f);
    EXPECT_EQ(event.MouseWheel.WheelY, -1.f);
}

TIMGUI_TEST(add_event, text) {
    ScopedContext const context{};
    modifier_keys modifiers{};

    add_event(
        ImGui::GetIO(),
        cgfw::TextEvent{.text = "\xC3\xA9", .time = cth::chrono::clock_t::now()},
        modifiers
    );

    ASSERT_EQ(context.events().Size, 1);
    auto const& event = context.events()[0];
    EXPECT_EQ(event.Type, ImGuiInputEventType_Text);
    EXPECT_EQ(event.Text.Char, 0xE9u);
}

TIMGUI_TEST(add_event, focus) {
    ScopedContext const context{};
    modifier_keys modifiers{};

    // a new context has the focus: a gain is no change
    add_event(
        ImGui::GetIO(),
        cgfw::FocusEvent{.focused = false, .time = cth::chrono::clock_t::now()},
        modifiers
    );

    ASSERT_EQ(context.events().Size, 1);
    auto const& event = context.events()[0];
    EXPECT_EQ(event.Type, ImGuiInputEventType_Focus);
    EXPECT_FALSE(event.AppFocused.Focused);
}

TIMGUI_TEST(add_event, focus_loss_forgets_the_modifier_keys) {
    ScopedContext const context{};
    modifier_keys modifiers{};
    auto& io = ImGui::GetIO();

    add_event(io, key_event(Key::SHIFT, false, true), modifiers);
    add_event(
        io,
        cgfw::FocusEvent{.focused = false, .time = cth::chrono::clock_t::now()},
        modifiers
    );

    // the left shift went up without focus
    EXPECT_TRUE(modifiers.set(ImGuiKey_RightShift, true));
    EXPECT_FALSE(modifiers.set(ImGuiKey_RightShift, false));
}

TIMGUI_TEST(add_event, no_imgui_key_adds_nothing) {
    ScopedContext const context{};
    modifier_keys modifiers{};

    add_event(ImGui::GetIO(), key_event(Key::CUSTOM_0, false, true), modifiers);
    add_event(ImGui::GetIO(), axis_event(Axis::CUSTOM_0, 1.f, 1.f), modifiers);

    EXPECT_EQ(context.events().Size, 0);
}
}
