#pragma once
#include <volk.h>

#include <cth/os/osdef.hpp>

#include <array>
#include <string_view>

namespace cvk::constants {
/**
 * handle type of @ref Fence::extractOsHandle()
 * @details windows: opaque win32 handle
 * @details posix: sync fd, pollable
 */
inline constexpr VkExternalFenceHandleTypeFlagBits OS_FENCE_HANDLE_TYPE =
#ifdef CTH_PLATFORM_WINDOWS
    VK_EXTERNAL_FENCE_HANDLE_TYPE_OPAQUE_WIN32_BIT;
#else
    VK_EXTERNAL_FENCE_HANDLE_TYPE_SYNC_FD_BIT;
#endif

/**
 * external fence features of @ref OS_FENCE_HANDLE_TYPE that @ref Fence::extractOsHandle() requires
 * @details posix: the import gives the fence its state back after the export
 */
inline constexpr VkExternalFenceFeatureFlags OS_FENCE_FEATURES =
#ifdef CTH_PLATFORM_WINDOWS
    VK_EXTERNAL_FENCE_FEATURE_EXPORTABLE_BIT;
#else
    VK_EXTERNAL_FENCE_FEATURE_EXPORTABLE_BIT | VK_EXTERNAL_FENCE_FEATURE_IMPORTABLE_BIT;
#endif

/**
 * device extensions that @ref Fence::extractOsHandle() requires
 */
inline constexpr std::array<std::string_view, 1> OS_FENCE_DEVICE_EXTENSIONS{
#ifdef CTH_PLATFORM_WINDOWS
    "VK_KHR_external_fence_win32",
#else
    VK_KHR_EXTERNAL_FENCE_FD_EXTENSION_NAME,
#endif
};
}
