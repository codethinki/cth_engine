#pragma once
#include <cstdint>

namespace cth {
struct Constants {
    Constants() = delete;

    static constexpr uint32_t ALL = std::numeric_limits<uint32_t>::max();
    static constexpr size_t WHOLE_SIZE = VK_WHOLE_SIZE;

    static constexpr VkAccessFlags DEFAULT_ACCESS = VK_ACCESS_NONE;
    static constexpr auto QUEUE_FAMILY_IGNORED = VK_QUEUE_FAMILY_IGNORED;
    static constexpr VkPipelineStageFlags PIPELINE_STAGE_IGNORED = VK_PIPELINE_STAGE_FLAG_BITS_MAX_ENUM;

    static constexpr auto ASPECT_MASK_IGNORED = VK_IMAGE_ASPECT_FLAG_BITS_MAX_ENUM;
    static constexpr auto IMAGE_LAYOUT_IGNORED = VK_IMAGE_LAYOUT_MAX_ENUM;
};
}
