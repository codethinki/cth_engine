#pragma once
#include "jvk/utility/os/os_def.hpp"

#ifdef JVK_PLATFORM_WINDOWS
#include <cth/win/screen.hpp>

namespace jvk::os {
using window_t = cth::win::window_t;
}

#elifdef JVK_PLATFORM_LINUX
#error "no support for linux yet"
#elifdef JVK_PLATFORM_ANDROID
#error "no support for android yet"
#endif
