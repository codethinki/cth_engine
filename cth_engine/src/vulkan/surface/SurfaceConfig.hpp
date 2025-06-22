#pragma once
#include <array>
#include <volk.h>

namespace cth::vk {
struct SurfaceConfig {
    using format_t = VkSurfaceFormatKHR;
    using present_mode_t = VkPresentModeKHR;
    static cxpr std::array<format_t, 1> DEFAULT_ALLOWED_FORMATS{{{VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}}};
    static cxpr std::array<present_mode_t, 1> DEFAULT_ALLOWED_PRESENT_MODES{{VK_PRESENT_MODE_FIFO_KHR}};

    /**
     * @brief formats allowed by the surface sorted by decreasing preference
     * @details empty -> don't care
     */
    std::vector<VkSurfaceFormatKHR> allowedSurfaceFormats{std::from_range, DEFAULT_ALLOWED_FORMATS};

    std::vector<VkPresentModeKHR> allowedPresentModes{std::from_range, DEFAULT_ALLOWED_PRESENT_MODES};
};
}
