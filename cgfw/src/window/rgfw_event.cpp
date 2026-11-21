#include "window/rgfw_event.hpp"

#include <cth/os/osdef.hpp>

#ifdef CTH_PLATFORM_WINDOWS
#include <windows.h>
#else
#include <time.h>
#endif


namespace cgfw::dev {
uint32_t os_input_time() {
#ifdef CTH_PLATFORM_WINDOWS
    return static_cast<uint32_t>(GetTickCount());
#else
    timespec time{};
    clock_gettime(CLOCK_MONOTONIC, &time);

    // the input times wrap at 2^32 milliseconds, the truncation wraps the same
    auto const milliseconds = static_cast<uint64_t>(time.tv_sec) * 1000
        + static_cast<uint64_t>(time.tv_nsec) / 1'000'000;
    return static_cast<uint32_t>(milliseconds);
#endif
}

std::optional<window_event> to_event(
    RGFW_event const& rgfwEvent,
    input_clock const& clock,
    u32 (*keyChar)(RGFW_key)
) {
    using cth::io::Axis;
    using cth::io::axis_state;
    using cth::io::axis_update;
    using cth::io::key_state;
    using cth::io::key_update;

    switch(rgfwEvent.common.type) {
        case RGFW_keyPressed:
        case RGFW_keyReleased: {
            auto const& key = rgfwEvent.key;
            auto const exKey = to_ex_key(key.value);
            if(!exKey)
                return std::nullopt;

            return key_update{
                .data = key_state{
                    *exKey,
                    key.state != RGFW_FALSE,
                    to_utf8(static_cast<char32_t>(keyChar(key.value))),
                    key.repeat != RGFW_FALSE
                },
                .time = clock.timePoint(key.time)
            };
        }
        case RGFW_mouseButtonPressed:
        case RGFW_mouseButtonReleased: {
            auto const& button = rgfwEvent.button;
            auto const key = to_mouse_key(button.value);
            if(!key)
                return std::nullopt;

            return key_update{
                .data = key_state{*key, false, button.state != RGFW_FALSE},
                .time = clock.timePoint(button.time)
            };
        }
        case RGFW_mouseMotion: {
            auto const& mouse = rgfwEvent.mouse;
            return axis_update{
                .data = axis_state{
                    .axis = Axis::MOUSE_POSITION,
                    .value = {static_cast<float>(mouse.x), static_cast<float>(mouse.y)}
                },
                .time = clock.timePoint(mouse.time)
            };
        }
        case RGFW_mouseScroll: {
            auto const& delta = rgfwEvent.delta;
            return axis_update{
                .data = axis_state{.axis = Axis::MOUSE_SCROLL, .value = {delta.x, delta.y}},
                .time = clock.timePoint(delta.time)
            };
        }
        case RGFW_keyChar: {
            auto const& keyCharEvent = rgfwEvent.keyChar;
            auto text = to_utf8(static_cast<char32_t>(keyCharEvent.value));
            if(text.empty())
                return std::nullopt;

            return TextEvent{.text = std::move(text), .time = clock.timePoint(keyCharEvent.time)};
        }
        // RGFW gives focus events no OS time
        case RGFW_windowFocusIn: return FocusEvent{.focused = true, .time = clock.now};
        case RGFW_windowFocusOut: return FocusEvent{.focused = false, .time = clock.now};

        default: return std::nullopt;
    }
}
}
