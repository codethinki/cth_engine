#pragma once
#include <volk.h>

#include <array>
#include <vector>

namespace cvk {
struct SurfaceConfig {
    using format_t = VkSurfaceFormatKHR;
    using present_mode_t = VkPresentModeKHR;
    static constexpr std::array<format_t, 1> DEFAULT_ALLOWED_FORMATS{
        {{VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}}
    };
    static constexpr std::array<present_mode_t, 1> DEFAULT_ALLOWED_PRESENT_MODES{{VK_PRESENT_MODE_FIFO_KHR}};

    /**
     * @brief formats the surface allows, the first is the most preferred
     * @details empty -> every available format
     */
    std::vector<format_t> allowedSurfaceFormats{std::from_range, DEFAULT_ALLOWED_FORMATS};

    /**
     * @brief present modes the surface allows, the first is the most preferred
     * @details empty -> every available present mode
     */
    std::vector<present_mode_t> allowedPresentModes{std::from_range, DEFAULT_ALLOWED_PRESENT_MODES};
};
}
