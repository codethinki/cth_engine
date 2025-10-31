#include "jvk/utility/os/os_def.hpp"

#ifdef JVK_PLATFORM_WINDOWS
#include "win/win_window.impl"
#elifdef JVK_PLATFORM_LINUX
#error "no window support for linux yet"
#elifdef JVK_PLATFORM_ANDROID
#error "no window support for android yet"
#else
#error "unknown platform"
#endif