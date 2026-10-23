#pragma once
#include <volk.h>

#include <optional>


namespace cvk {
/**
 * @brief sampler state, defaults: linear filtering, repeat, 16x anisotropy
 */
struct SamplerConfig {
    VkFilter magFilter = VK_FILTER_LINEAR;
    VkFilter minFilter = VK_FILTER_LINEAR;
    VkSamplerMipmapMode mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    VkSamplerAddressMode addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    VkSamplerAddressMode addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    VkSamplerAddressMode addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

    float mipLodBias = 0.f;
    float minLod = 0.f;
    /**
     * @brief VK_LOD_CLAMP_NONE -> no clamp
     */
    float maxLod = VK_LOD_CLAMP_NONE;

    /**
     * @brief 0 -> no anisotropic filtering
     */
    float maxAnisotropy = 16.f;

    /**
     * @brief std::nullopt -> no comparison
     */
    std::optional<VkCompareOp> compareOp = std::nullopt;

    VkBorderColor borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    VkBool32 unnormalizedCoordinates = VK_FALSE;

    [[nodiscard]] constexpr VkSamplerCreateInfo createInfo() const {
        return VkSamplerCreateInfo{
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .magFilter = magFilter,
            .minFilter = minFilter,
            .mipmapMode = mipmapMode,
            .addressModeU = addressModeU,
            .addressModeV = addressModeV,
            .addressModeW = addressModeW,
            .mipLodBias = mipLodBias,
            .anisotropyEnable = maxAnisotropy != 0.f ? VK_TRUE : VK_FALSE,
            .maxAnisotropy = maxAnisotropy,
            .compareEnable = compareOp.has_value() ? VK_TRUE : VK_FALSE,
            .compareOp = compareOp.value_or(VK_COMPARE_OP_NEVER),
            .minLod = minLod,
            .maxLod = maxLod,
            .borderColor = borderColor,
            .unnormalizedCoordinates = unnormalizedCoordinates,
        };
    }
};
}
