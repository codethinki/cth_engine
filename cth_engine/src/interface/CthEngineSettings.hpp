#pragma once
#include <cstdint>
#include <vulkan/vulkan.h>

namespace cth {
    inline constexpr VkSampleCountFlagBits MAX_MSAA_SAMPLES = VK_SAMPLE_COUNT_4_BIT;
    inline constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;
}
