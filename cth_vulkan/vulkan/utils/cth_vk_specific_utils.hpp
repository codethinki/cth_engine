#pragma once

#include <vulkan/vulkan_core.h>

#include <array>

namespace cth {

[[nodiscard]] inline std::array<VkBool32, 55> deviceFeaturesToArray(const VkPhysicalDeviceFeatures& features) {
    return std::array{
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
} // namespace cth
