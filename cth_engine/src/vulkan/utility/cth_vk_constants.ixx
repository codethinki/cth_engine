module;
#include <cstdint>
#include <lib/volk.hpp>
export module cth.vk.constants;

export import cth.vk.constants.debug;
export import cth.vk.constants.device;

export namespace cth::vk {
struct create_t {};

constexpr create_t create{};

}

export namespace cth::vk::constants {


constexpr bool ENABLE_VALIDATION_LAYERS = DEBUG_MODE;

constexpr uint32_t ALL = std::numeric_limits<uint32_t>::max();
constexpr size_t WHOLE_SIZE = VK_WHOLE_SIZE;

constexpr VkAccessFlags DEFAULT_ACCESS = VK_ACCESS_NONE;
constexpr auto QUEUE_FAMILY_IGNORED = VK_QUEUE_FAMILY_IGNORED;
constexpr VkPipelineStageFlags PIPELINE_STAGE_IGNORED = VK_PIPELINE_STAGE_FLAG_BITS_MAX_ENUM;

constexpr auto ASPECT_MASK_IGNORED = VK_IMAGE_ASPECT_FLAG_BITS_MAX_ENUM;
constexpr auto IMAGE_LAYOUT_IGNORED = VK_IMAGE_LAYOUT_MAX_ENUM;

constexpr VkSampleCountFlagBits MAX_MSAA_SAMPLES = VK_SAMPLE_COUNT_4_BIT;
constexpr size_t FRAMES_IN_FLIGHT = 2;

}
