#pragma once
#include "jvk/utility/os/os_def.hpp"


namespace jvk::os {
enum class Platform {
    WINDOWS,
    LINUX,
    ANDROID,
    UNKNOWN
};

constexpr auto PLATFORM =
#ifdef JVK_PLATFORM_WINDOWS
    Platform::WINDOWS;
#elifdef JVK_PLATFORM_LINUX
Platform::LINUX;
#elifdef JVK_PLATFORM_ANDROID
Platform::ANDROID;
#else
Platform::UNKNOWN;
#endif

enum class Fs {
    WINDOWS,
    POSIX,
    UNKNOWN
};

constexpr auto FILE_SYSTEM =
#ifdef JVK_FS_WINDOWS
    Fs::WINDOWS;
#elifdef JVK_FS_POSIX
Fs::POSIX;
#else
Fs::UNKNOWN;
#endif
}