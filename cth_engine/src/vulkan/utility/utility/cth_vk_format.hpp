#pragma once
#include <vulkan/vulkan.h>

namespace cth::vk::fmt {
constexpr std::string_view to_string(VkResult result) {
    switch(result) {
        case VK_SUCCESS: return "VK_SUCCESS";
        case VK_NOT_READY: return "VK_NOT_READY";
        case VK_TIMEOUT: return "VK_TIMEOUT";
        case VK_EVENT_SET: return "VK_EVENT_SET";
        case VK_EVENT_RESET: return "VK_EVENT_RESET";
        case VK_INCOMPLETE: return "VK_INCOMPLETE";
        case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
        case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
        case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
        case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
        case VK_ERROR_MEMORY_MAP_FAILED: return "VK_ERROR_MEMORY_MAP_FAILED";
        case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT";
        case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
        case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
        case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
        case VK_ERROR_TOO_MANY_OBJECTS: return "VK_ERROR_TOO_MANY_OBJECTS";
        case VK_ERROR_FORMAT_NOT_SUPPORTED: return "VK_ERROR_FORMAT_NOT_SUPPORTED";
        case VK_ERROR_FRAGMENTED_POOL: return "VK_ERROR_FRAGMENTED_POOL";
        case VK_ERROR_UNKNOWN: return "VK_ERROR_UNKNOWN";
        case VK_ERROR_OUT_OF_POOL_MEMORY: return "VK_ERROR_OUT_OF_POOL_MEMORY";
        case VK_ERROR_INVALID_EXTERNAL_HANDLE: return "VK_ERROR_INVALID_EXTERNAL_HANDLE";
        case VK_ERROR_FRAGMENTATION: return "VK_ERROR_FRAGMENTATION";
        case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS: return "VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS";
        case VK_PIPELINE_COMPILE_REQUIRED: return "VK_PIPELINE_COMPILE_REQUIRED";
        case VK_ERROR_SURFACE_LOST_KHR: return "VK_ERROR_SURFACE_LOST_KHR";
        case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
        case VK_SUBOPTIMAL_KHR: return "VK_SUBOPTIMAL_KHR";
        case VK_ERROR_OUT_OF_DATE_KHR: return "VK_ERROR_OUT_OF_DATE_KHR";
        case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR: return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";
        case VK_ERROR_VALIDATION_FAILED_EXT: return "VK_ERROR_VALIDATION_FAILED_EXT";
        case VK_ERROR_INVALID_SHADER_NV: return "VK_ERROR_INVALID_SHADER_NV";
        case VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR: return "VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR";
        case VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR: return "VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR";
        case VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR: return "VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR";
        case VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR: return "VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR";
        case VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR: return "VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR";
        case VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR: return "VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR";
        case VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT: return "VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT";
        case VK_ERROR_NOT_PERMITTED_KHR: return "VK_ERROR_NOT_PERMITTED_KHR";
        case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT: return "VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT";
        case VK_THREAD_IDLE_KHR: return "VK_THREAD_IDLE_KHR";
        case VK_THREAD_DONE_KHR: return "VK_THREAD_DONE_KHR";
        case VK_OPERATION_DEFERRED_KHR: return "VK_OPERATION_DEFERRED_KHR";
        case VK_OPERATION_NOT_DEFERRED_KHR: return "VK_OPERATION_NOT_DEFERRED_KHR";
        case VK_ERROR_INVALID_VIDEO_STD_PARAMETERS_KHR: return "VK_ERROR_INVALID_VIDEO_STD_PARAMETERS_KHR";
        case VK_ERROR_COMPRESSION_EXHAUSTED_EXT: return "VK_ERROR_COMPRESSION_EXHAUSTED_EXT";
        case VK_ERROR_INCOMPATIBLE_SHADER_BINARY_EXT: return "VK_ERROR_INCOMPATIBLE_SHADER_BINARY_EXT";
        case VK_RESULT_MAX_ENUM:
        default: return "UNKNOWN VK_RESULT (BUG)";
    }
}
constexpr std::string_view to_string(VkDescriptorType type) {
    switch(type) {
        case VK_DESCRIPTOR_TYPE_SAMPLER: return "VK_DESCRIPTOR_TYPE_SAMPLER";
        case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER: return "VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER";
        case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE: return "VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE";
        case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE: return "VK_DESCRIPTOR_TYPE_STORAGE_IMAGE";
        case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER: return "VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER";
        case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER: return "VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER";
        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER: return "VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER";
        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER: return "VK_DESCRIPTOR_TYPE_STORAGE_BUFFER";
        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC: return "VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC";
        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC: return "VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC";
        case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT: return "VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT";
        case VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK: return "VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK";
        case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR: return "VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR";
        case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV: return "VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV";
        case VK_DESCRIPTOR_TYPE_SAMPLE_WEIGHT_IMAGE_QCOM: return "VK_DESCRIPTOR_TYPE_SAMPLE_WEIGHT_IMAGE_QCOM";
        case VK_DESCRIPTOR_TYPE_BLOCK_MATCH_IMAGE_QCOM: return "VK_DESCRIPTOR_TYPE_BLOCK_MATCH_IMAGE_QCOM";
        case VK_DESCRIPTOR_TYPE_MUTABLE_EXT: return "VK_DESCRIPTOR_TYPE_MUTABLE_EXT";
        case VK_DESCRIPTOR_TYPE_MAX_ENUM: return "VK_DESCRIPTOR_TYPE_MAX_ENUM";
        default: return "UNKNOWN DESCRIPTOR_TYPE (BUG)";
    }
}
}

//VkResult
template<>
struct std::formatter<VkResult> : std::formatter<int> {
    constexpr auto parse(format_parse_context& ctx) { return std::formatter<int>::parse(ctx); }

    template<typename FormatContext>
    auto format(VkResult const& obj, FormatContext& ctx) const {
        return format_to(ctx.out(), "VkResult [{}]", cth::vk::fmt::to_string(obj));
    }
};


//VkDescriptorType
template<>
struct std::formatter<VkDescriptorType> : std::formatter<int> {
    constexpr auto parse(format_parse_context& ctx) { return std::formatter<int>::parse(ctx); }

    template<typename FormatContext>
    auto format(VkDescriptorType const& obj, FormatContext& ctx) const {
        return format_to(ctx.out(), "[{}]", cth::vk::fmt::to_string(obj));
    }
};

//VkStructureType

template<>
struct std::formatter<VkStructureType> : std::formatter<int> {
    constexpr auto parse(format_parse_context& ctx) { return std::formatter<int>::parse(ctx); }

    template<typename FormatContext>
    auto format(VkStructureType const& obj, FormatContext& ctx) const {
        return format_to(ctx.out(), "VkStructureType(value={})", static_cast<uint32_t>(obj));
    }
};
