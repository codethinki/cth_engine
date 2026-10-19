#pragma once
#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <functional>
#include <string_view>
#include <vector>


namespace cvk {
/**
 * @brief feature bits of a device, one bool per Vulkan feature
 * @details the fields follow the Vulkan feature structs in order, see @ref dev::feature_structs,
 *  a true bit of an extension struct enables its extension, see @ref dev::extensions_of()
 */
struct DeviceFeatures {
    // Vulkan 1.0, VkPhysicalDeviceFeatures
    bool robustBufferAccess = false;
    bool fullDrawIndexUint32 = false;
    bool imageCubeArray = false;
    bool independentBlend = false;
    bool geometryShader = false;
    bool tessellationShader = false;
    bool sampleRateShading = false;
    bool dualSrcBlend = false;
    bool logicOp = false;
    bool multiDrawIndirect = false;
    bool drawIndirectFirstInstance = false;
    bool depthClamp = false;
    bool depthBiasClamp = false;
    bool fillModeNonSolid = false;
    bool depthBounds = false;
    bool wideLines = false;
    bool largePoints = false;
    bool alphaToOne = false;
    bool multiViewport = false;
    bool samplerAnisotropy = false;
    bool textureCompressionETC2 = false;
    bool textureCompressionASTC_LDR = false;
    bool textureCompressionBC = false;
    bool occlusionQueryPrecise = false;
    bool pipelineStatisticsQuery = false;
    bool vertexPipelineStoresAndAtomics = false;
    bool fragmentStoresAndAtomics = false;
    bool shaderTessellationAndGeometryPointSize = false;
    bool shaderImageGatherExtended = false;
    bool shaderStorageImageExtendedFormats = false;
    bool shaderStorageImageMultisample = false;
    bool shaderStorageImageReadWithoutFormat = false;
    bool shaderStorageImageWriteWithoutFormat = false;
    bool shaderUniformBufferArrayDynamicIndexing = false;
    bool shaderSampledImageArrayDynamicIndexing = false;
    bool shaderStorageBufferArrayDynamicIndexing = false;
    bool shaderStorageImageArrayDynamicIndexing = false;
    bool shaderClipDistance = false;
    bool shaderCullDistance = false;
    bool shaderFloat64 = false;
    bool shaderInt64 = false;
    bool shaderInt16 = false;
    bool shaderResourceResidency = false;
    bool shaderResourceMinLod = false;
    bool sparseBinding = false;
    bool sparseResidencyBuffer = false;
    bool sparseResidencyImage2D = false;
    bool sparseResidencyImage3D = false;
    bool sparseResidency2Samples = false;
    bool sparseResidency4Samples = false;
    bool sparseResidency8Samples = false;
    bool sparseResidency16Samples = false;
    bool sparseResidencyAliased = false;
    bool variableMultisampleRate = false;
    bool inheritedQueries = false;

    // Vulkan 1.1, VkPhysicalDeviceVulkan11Features
    bool storageBuffer16BitAccess = false;
    bool uniformAndStorageBuffer16BitAccess = false;
    bool storagePushConstant16 = false;
    bool storageInputOutput16 = false;
    bool multiview = false;
    bool multiviewGeometryShader = false;
    bool multiviewTessellationShader = false;
    bool variablePointersStorageBuffer = false;
    bool variablePointers = false;
    bool protectedMemory = false;
    bool samplerYcbcrConversion = false;
    bool shaderDrawParameters = false;

    // Vulkan 1.2, VkPhysicalDeviceVulkan12Features
    bool samplerMirrorClampToEdge = false;
    bool drawIndirectCount = false;
    bool storageBuffer8BitAccess = false;
    bool uniformAndStorageBuffer8BitAccess = false;
    bool storagePushConstant8 = false;
    bool shaderBufferInt64Atomics = false;
    bool shaderSharedInt64Atomics = false;
    bool shaderFloat16 = false;
    bool shaderInt8 = false;
    bool descriptorIndexing = false;
    bool shaderInputAttachmentArrayDynamicIndexing = false;
    bool shaderUniformTexelBufferArrayDynamicIndexing = false;
    bool shaderStorageTexelBufferArrayDynamicIndexing = false;
    bool shaderUniformBufferArrayNonUniformIndexing = false;
    bool shaderSampledImageArrayNonUniformIndexing = false;
    bool shaderStorageBufferArrayNonUniformIndexing = false;
    bool shaderStorageImageArrayNonUniformIndexing = false;
    bool shaderInputAttachmentArrayNonUniformIndexing = false;
    bool shaderUniformTexelBufferArrayNonUniformIndexing = false;
    bool shaderStorageTexelBufferArrayNonUniformIndexing = false;
    bool descriptorBindingUniformBufferUpdateAfterBind = false;
    bool descriptorBindingSampledImageUpdateAfterBind = false;
    bool descriptorBindingStorageImageUpdateAfterBind = false;
    bool descriptorBindingStorageBufferUpdateAfterBind = false;
    bool descriptorBindingUniformTexelBufferUpdateAfterBind = false;
    bool descriptorBindingStorageTexelBufferUpdateAfterBind = false;
    bool descriptorBindingUpdateUnusedWhilePending = false;
    bool descriptorBindingPartiallyBound = false;
    bool descriptorBindingVariableDescriptorCount = false;
    bool runtimeDescriptorArray = false;
    bool samplerFilterMinmax = false;
    bool scalarBlockLayout = false;
    bool imagelessFramebuffer = false;
    bool uniformBufferStandardLayout = false;
    bool shaderSubgroupExtendedTypes = false;
    bool separateDepthStencilLayouts = false;
    bool hostQueryReset = false;
    bool timelineSemaphore = false;
    bool bufferDeviceAddress = false;
    bool bufferDeviceAddressCaptureReplay = false;
    bool bufferDeviceAddressMultiDevice = false;
    bool vulkanMemoryModel = false;
    bool vulkanMemoryModelDeviceScope = false;
    bool vulkanMemoryModelAvailabilityVisibilityChains = false;
    bool shaderOutputViewportIndex = false;
    bool shaderOutputLayer = false;
    bool subgroupBroadcastDynamicId = false;

    // Vulkan 1.3, VkPhysicalDeviceVulkan13Features
    bool robustImageAccess = false;
    bool inlineUniformBlock = false;
    bool descriptorBindingInlineUniformBlockUpdateAfterBind = false;
    bool pipelineCreationCacheControl = false;
    bool privateData = false;
    bool shaderDemoteToHelperInvocation = false;
    bool shaderTerminateInvocation = false;
    bool subgroupSizeControl = false;
    bool computeFullSubgroups = false;
    bool synchronization2 = false;
    bool textureCompressionASTC_HDR = false;
    bool shaderZeroInitializeWorkgroupMemory = false;
    bool dynamicRendering = false;
    bool shaderIntegerDotProduct = false;
    bool maintenance4 = false;

    // VK_KHR_swapchain_maintenance1, VkPhysicalDeviceSwapchainMaintenance1FeaturesKHR
    bool swapchainMaintenance1 = false;

    // VK_KHR_dynamic_rendering_local_read, VkPhysicalDeviceDynamicRenderingLocalReadFeaturesKHR
    bool dynamicRenderingLocalRead = false;

    /**
     * @brief enables every feature of other
     */
    constexpr DeviceFeatures& merge(DeviceFeatures const& other);

    /**
     * @return names of the features that required enables and this does not
     */
    [[nodiscard]] std::vector<std::string_view> missing(DeviceFeatures const& required) const;

    [[nodiscard]] constexpr bool operator==(DeviceFeatures const&) const = default;
};
}

namespace cvk::dev {
/**
 * @brief feature bits in @ref DeviceFeatures, every field is a bool without padding
 */
inline constexpr size_t FEATURE_COUNT = sizeof(DeviceFeatures);

using feature_bits_t = std::array<bool, FEATURE_COUNT>;

/**
 * @return field i of the features as element i
 */
[[nodiscard]] constexpr feature_bits_t to_bits(DeviceFeatures const& features) {
    return std::bit_cast<feature_bits_t>(features);
}

/**
 * @return element i of the bits as field i
 */
[[nodiscard]] constexpr DeviceFeatures to_features(feature_bits_t const& bits) {
    return std::bit_cast<DeviceFeatures>(bits);
}
}

namespace cvk {
constexpr DeviceFeatures& DeviceFeatures::merge(DeviceFeatures const& other) {
    auto bits = dev::to_bits(*this);
    std::ranges::transform(bits, dev::to_bits(other), bits.begin(), std::logical_or{});

    *this = dev::to_features(bits);
    return *this;
}
}
