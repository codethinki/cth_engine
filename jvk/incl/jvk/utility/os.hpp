#pragma once

#include <cth/win/screen.hpp>

namespace jvk {
class Instance;
class Surface;
}

namespace jvk::os {

class os_exception : public ::cth::except::default_exception {
public:
    explicit os_exception(
        std::string_view msg,
        except::Severity const severity = except::ERR,
        std::source_location loc = {},
        std::stacktrace trace = {}
    )
        : default_exception{std::format("jvk_os: {}", msg), severity, std::move(loc), std::move(trace)} {}

};

}

#define JVK_STABLE_OS_THROW(expression, msg, ...)\
    CTH_STABLE_THROW_T(jvk::os::os_exception, expression, msg, __VA_ARGS__)

namespace jvk::os {
using window_t = cth::win::window_t;

/**
 * @brief creates an operating system specific hidden surface for temporary use
 * @param instance instance to create for
 * @return os specific surface
 * @throws jvk::result_exception if surface creation fails
 */
[[nodiscard]] Surface surface_from_window(Instance const& instance, window_t const& window);

/**
 * Creates one window per monitor
 * @throws jvk::os::os_exception if no monitors are found
 * @throws jvk::os::os_exception if window construction fails
 */
[[nodiscard]] std::vector<window_t> create_hidden_monitor_windows();


}
