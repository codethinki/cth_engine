#include "cvk/data/img/sampler_config.hpp"


namespace cvk {
VkSamplerCreateInfo SamplerConfig::createInfo() const {
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
}
