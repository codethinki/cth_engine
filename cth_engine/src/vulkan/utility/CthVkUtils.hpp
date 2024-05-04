#pragma once
#include <cth/cth_exception.hpp>

#include <vulkan/vulkan.h>


#include <algorithm>
#include <array>


namespace cth::utils {

[[nodiscard]] inline std::array<bool, 55> deviceFeaturesToArray(const VkPhysicalDeviceFeatures& features) {
    std::array<VkBool32, 55> arr{
        features.robustBufferAccess,
        features.fullDrawIndexUint32,
        features.imageCubeArray,
        features.independentBlend,
        features.geometryShader,
        features.tessellationShader,
        features.sampleRateShading,
        features.dualSrcBlend,
        features.logicOp,
        features.multiDrawIndirect,
        features.drawIndirectFirstInstance,
        features.depthClamp,
        features.depthBiasClamp,
        features.fillModeNonSolid,
        features.depthBounds,
        features.wideLines,
        features.largePoints,
        features.alphaToOne,
        features.multiViewport,
        features.samplerAnisotropy,
        features.textureCompressionETC2,
        features.textureCompressionASTC_LDR,
        features.textureCompressionBC,
        features.occlusionQueryPrecise,
        features.pipelineStatisticsQuery,
        features.vertexPipelineStoresAndAtomics,
        features.fragmentStoresAndAtomics,
        features.shaderTessellationAndGeometryPointSize,
        features.shaderImageGatherExtended,
        features.shaderStorageImageExtendedFormats,
        features.shaderStorageImageMultisample,
        features.shaderStorageImageReadWithoutFormat,
        features.shaderStorageImageWriteWithoutFormat,
        features.shaderUniformBufferArrayDynamicIndexing,
        features.shaderSampledImageArrayDynamicIndexing,
        features.shaderStorageBufferArrayDynamicIndexing,
        features.shaderStorageImageArrayDynamicIndexing,
        features.shaderClipDistance,
        features.shaderCullDistance,
        features.shaderFloat64,
        features.shaderInt64,
        features.shaderInt16,
        features.shaderResourceResidency,
        features.shaderResourceMinLod,
        features.sparseBinding,
        features.sparseResidencyBuffer,
        features.sparseResidencyImage2D,
        features.sparseResidencyImage3D,
        features.sparseResidency2Samples,
        features.sparseResidency4Samples,
        features.sparseResidency8Samples,
        features.sparseResidency16Samples,
        features.sparseResidencyAliased,
        features.variableMultisampleRate,
        features.inheritedQueries
    };

    std::array<bool, 55> ret{};
    std::ranges::transform(arr, ret.begin(), [](const VkBool32& b) { return b == VK_TRUE; });

    return ret;
};
[[nodiscard]] constexpr std::string_view deviceFeatureIndexToString(const size_t index) {
    switch(index) {
        case 0: return "robustBufferAccess";
        case 1: return "fullDrawIndexUint32";
        case 2: return "imageCubeArray";
        case 3: return "independentBlend";
        case 4: return "geometryShader";
        case 5: return "tessellationShader";
        case 6: return "sampleRateShading";
        case 7: return "dualSrcBlend";
        case 8: return "logicOp";
        case 9: return "multiDrawIndirect";
        case 10: return "drawIndirectFirstInstance";
        case 11: return "depthClamp";
        case 12: return "depthBiasClamp";
        case 13: return "fillModeNonSolid";
        case 14: return "depthBounds";
        case 15: return "wideLines";
        case 16: return "largePoints";
        case 17: return "alphaToOne";
        case 18: return "multiViewport";
        case 19: return "samplerAnisotropy";
        case 20: return "textureCompressionETC2";
        case 21: return "textureCompressionASTC_LDR";
        case 22: return "textureCompressionBC";
        case 23: return "occlusionQueryPrecise";
        case 24: return "pipelineStatisticsQuery";
        case 25: return "vertexPipelineStoresAndAtomics";
        case 26: return "fragmentStoresAndAtomics";
        case 27: return "shaderTessellationAndGeometryPointSize";
        case 28: return "shaderImageGatherExtended";
        case 29: return "shaderStorageImageExtendedFormats";
        case 30: return "shaderStorageImageMultisample";
        case 31: return "shaderStorageImageReadWithoutFormat";
        case 32: return "shaderStorageImageWriteWithoutFormat";
        case 33: return "shaderUniformBufferArrayDynamicIndexing";
        case 34: return "shaderSampledImageArrayDynamicIndexing";
        case 35: return "shaderStorageBufferArrayDynamicIndexing";
        case 36: return "shaderStorageImageArrayDynamicIndexing";
        case 37: return "shaderClipDistance";
        case 38: return "shaderCullDistance";
        case 39: return "shaderFloat64";
        case 40: return "shaderInt64";
        case 41: return "shaderInt16";
        case 42: return "shaderResourceResidency";
        case 43: return "shaderResourceMinLod";
        case 44: return "sparseBinding";
        case 45: return "sparseResidencyBuffer";
        case 46: return "sparseResidencyImage2D";
        case 47: return "sparseResidencyImage3D";
        case 48: return "sparseResidency2Samples";
        case 49: return "sparseResidency4Samples";
        case 50: return "sparseResidency8Samples";
        case 51: return "sparseResidency16Samples";
        case 52: return "sparseResidencyAliased";
        case 53: return "variableMultisampleRate";
        case 54: return "inheritedQueries";
        default: return "Invalid index";
    }
}


inline std::string to_string(const VkResult result) {
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
        default: return "UNKNOWN_ERROR";
    }
}
inline std::string to_string(const VkDescriptorType type) {
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
        default: return "UNKNOWN_DESCRIPTOR_TYPE";
    }
}

[[nodiscard]] inline std::vector<const char*> toCharVec(const std::vector<std::string>& str_vec) {
    std::vector<const char*> charVec(str_vec.size());
    std::ranges::transform(str_vec, charVec.begin(), [](const std::string& str) { return str.c_str(); });

    return charVec;
}



} // namespace cth

namespace cth::except {
class vk_result_exception : public cth::except::default_exception {
public:
    vk_result_exception(const VkResult result, cth::except::default_exception ex) : default_exception(ex), _vkResult(result) {
        ex.add("error code {0} ({1})", utils::to_string(_vkResult), static_cast<int>(_vkResult));
    }
    [[nodiscard]] VkResult result() const noexcept { return _vkResult; }

private:
    VkResult _vkResult;
};
}
