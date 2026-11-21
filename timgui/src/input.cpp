#include "input.hpp"

#include <cth/meta/variant.hpp>

#include <variant>


namespace timgui::dev {
void add_event(ImGuiIO& io, cgfw::window_event const& event, modifier_keys& modifiers) {
    using cth::io::Axis;

    std::visit(
        cth::var::overload{
            [&](cth::io::key_update const& update) {
                auto const& state = update.data;
                if(auto const buttonOpt = to_imgui_button(state.exKey.key)) {
                    io.AddMouseButtonEvent(*buttonOpt, state.down);
                    return;
                }

                auto const key = to_imgui_key(state);
                if(key == ImGuiKey_None)
                    return;

                if(auto const mod = to_imgui_mod(key); mod != ImGuiMod_None)
                    io.AddKeyEvent(mod, modifiers.set(key, state.down));
                io.AddKeyEvent(key, state.down);
            },
            [&](cth::io::axis_update const& update) {
                auto const& [axis, value] = update.data;
                if(axis == Axis::MOUSE_POSITION)
                    io.AddMousePosEvent(value[0], value[1]);
                else if(axis == Axis::MOUSE_SCROLL)
                    io.AddMouseWheelEvent(value[0], value[1]);
            },
            [&](cgfw::TextEvent const& text) { io.AddInputCharactersUTF8(text.text.c_str()); },
            [&](cgfw::FocusEvent const& focus) {
                // ImGui forgets its keys at the loss: a release without focus sends no event
                if(!focus.focused)
                    modifiers = modifier_keys{};
                io.AddFocusEvent(focus.focused);
            },
        },
        event
    );
}
}
