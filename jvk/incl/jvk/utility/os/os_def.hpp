#pragma once
#if defined(WIN32) && !defined(JVK_PLATFORM_WINDOWS)
#define JVK_PLATFORM_WINDOWS
#endif

#if defined(__linux__) && !defined(JVK_PLATFORM_LINUX)
#define JVK_PLATFORM_LINUX
#endif

#if defined(__ANDROID__) && !defined(JVK_PLATFORM_ANDROID)
#define JVK_PLATFORM_ANDROID
#endif

#if defined(JVK_PLATFORM_WINDOWS) + defined(JVK_PLATFORM_LINUX) + defined(JVK_PLATFORM_ANDROID) != 1
#error "multiple or no platforms defined"
#endif


#if defined(JVK_PLATFORM_WINDOWS)
#define JVK_FS_WINDOWS
#elif defined(JVK_PLATFORM_LINUX) || defined(JVK_PLATFORM_ANDROID)
#define JVK_FS_POSIX
#endif

