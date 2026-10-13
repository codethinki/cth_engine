#pragma once
#include "jvk/utility/os/os_helper_include.hpp"

namespace jvk {
class Instance;
class Surface;
}

namespace jvk::os {

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