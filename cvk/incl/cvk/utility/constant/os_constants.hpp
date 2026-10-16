#pragma once
#include <volk.h>

#include <cth/os/osdef.hpp>

#include <array>
#include <string_view>

namespace cvk::constants {
/**
 * @brief handle type of @ref Fence::extractOsHandle()
 * @details windows: opaque win32 handle, posix: sync fd, pollable
 */
inline constexpr VkExternalFenceHandleTypeFlagBits OS_FENCE_HANDLE_TYPE =
#ifdef CTH_PLATFORM_WINDOWS
    VK_EXTERNAL_FENCE_HANDLE_TYPE_OPAQUE_WIN32_BIT;
#else
    VK_EXTERNAL_FENCE_HANDLE_TYPE_SYNC_FD_BIT;
#endif

/**
 * @brief device extensions that @ref Fence::extractOsHandle() requires
 */
inline constexpr std::array<std::string_view, 1> OS_FENCE_DEVICE_EXTENSIONS{
#ifdef CTH_PLATFORM_WINDOWS
    "VK_KHR_external_fence_win32",
#else
    VK_KHR_EXTERNAL_FENCE_FD_EXTENSION_NAME,
#endif
};
}
