#pragma once
#ifndef _FINAL
#define CONSTANT_DEBUG_MODE
#endif

#include <vulkan/vulkan.h>

#include <cstdint>
#include <limits>

namespace cth::vk::constant {

static constexpr bool DEBUG_MODE = []() {
#ifdef CONSTANT_DEBUG_MODE
    return true;
#else
        return false;
#endif
}();

static constexpr bool ENABLE_VALIDATION_LAYERS = DEBUG_MODE;

static constexpr uint32_t ALL = std::numeric_limits<uint32_t>::max();
static constexpr size_t WHOLE_SIZE = VK_WHOLE_SIZE;

static constexpr VkAccessFlags DEFAULT_ACCESS = VK_ACCESS_NONE;
static constexpr auto QUEUE_FAMILY_IGNORED = VK_QUEUE_FAMILY_IGNORED;
static constexpr VkPipelineStageFlags PIPELINE_STAGE_IGNORED = VK_PIPELINE_STAGE_FLAG_BITS_MAX_ENUM;

static constexpr auto ASPECT_MASK_IGNORED = VK_IMAGE_ASPECT_FLAG_BITS_MAX_ENUM;
static constexpr auto IMAGE_LAYOUT_IGNORED = VK_IMAGE_LAYOUT_MAX_ENUM;

static constexpr VkSampleCountFlagBits MAX_MSAA_SAMPLES = VK_SAMPLE_COUNT_4_BIT;
static constexpr size_t FRAMES_IN_FLIGHT = 2;



static constexpr VkDebugUtilsMessageSeverityFlagsEXT DEBUG_MESSAGE_SEVERITY = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
    VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
static constexpr VkDebugUtilsMessageTypeFlagsEXT DEBUG_MESSAGE_TYPE = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
    VK_DEBUG_UTILS_MESSAGE_TYPE_DEVICE_ADDRESS_BINDING_BIT_EXT |
    VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
    VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
} // namespace cth
