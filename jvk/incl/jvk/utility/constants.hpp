#pragma once
// ReSharper disable CppUnusedIncludeDirective
#include "constant/debug_constants.hpp"
#include "constant/device_constants.hpp"
// ReSharper restore CppUnusedIncludeDirective

#include <volk.h>
#include <cth/macro.hpp>

#include <limits>

#include <cth/constants.hpp>

namespace jvk {
struct create_t {};

static constexpr create_t create{};
}

namespace jvk::constants {


constexpr bool ENABLE_VALIDATION_LAYERS = DEBUG_MODE;

constexpr uint32_t ALL = std::numeric_limits<uint32_t>::max();
constexpr size_t WHOLE_SIZE = VK_WHOLE_SIZE;

constexpr VkAccessFlags DEFAULT_ACCESS = VK_ACCESS_NONE;
constexpr auto QUEUE_FAMILY_IGNORED = VK_QUEUE_FAMILY_IGNORED;
constexpr VkPipelineStageFlags PIPELINE_STAGE_IGNORED = VK_PIPELINE_STAGE_FLAG_BITS_MAX_ENUM;

constexpr auto ASPECT_MASK_IGNORED = VK_IMAGE_ASPECT_FLAG_BITS_MAX_ENUM;
constexpr auto IMAGE_LAYOUT_IGNORED = VK_IMAGE_LAYOUT_MAX_ENUM;

constexpr auto MAX_MSAA_SAMPLES = VK_SAMPLE_COUNT_4_BIT;
constexpr size_t FRAMES_IN_FLIGHT = 2;

cxpr size_t SWAPCHAIN_ATTACHMENT_INDEX = 0;
cxpr auto COMPILATION_MODE = cth::COMPILATION_MODE;

} // namespace jvk::constants
