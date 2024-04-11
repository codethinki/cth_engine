#include "CthSampler.hpp"

#include "vulkan/base/CthDevice.hpp"

#include <cth/cth_log.hpp>

#include "vulkan/utility/CthVkUtils.hpp"

//Sampler

namespace cth {

Sampler::Sampler(Device* device, const Config& config) : device(device) { create(config); }
Sampler::~Sampler() {
    vkDestroySampler(device->get(), vkSampler, nullptr);

    log::msg("destroyed sampler");
}

void Sampler::create(const Config& config) {
    const auto createInfo = config.createInfo();

    const VkResult createResult = vkCreateSampler(device->get(), &createInfo, nullptr, &vkSampler);

    CTH_STABLE_ERR(createResult != VK_SUCCESS, "failed to create sampler")
        throw except::vk_result_exception(createResult, details->exception());

    cth::log::msg("created sampler");
}



} // namespace cth

//Config

namespace cth {

Sampler::Config::Config(const VkSamplerCreateInfo& create_info) {

    filters[0] = create_info.minFilter;
    filters[1] = create_info.magFilter;

    addressModes[0] = create_info.addressModeU;
    addressModes[1] = create_info.addressModeV;
    addressModes[2] = create_info.addressModeW;

    mipmapMode = create_info.mipmapMode;
    lodBias = create_info.mipLodBias;
    lod[0] = create_info.minLod;
    lod[1] = create_info.maxLod;

    maxAnisotropy = create_info.maxAnisotropy;

    compareOp = create_info.compareOp;

    borderColor = create_info.borderColor;
    unnormalizedCoordinates = create_info.unnormalizedCoordinates;

}

VkSamplerCreateInfo Sampler::Config::createInfo() const {
    VkSamplerCreateInfo info{};

    info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    info.pNext = nullptr;

    info.minFilter = filters[0];
    info.magFilter = filters[1];

    info.addressModeU = addressModes[0];
    info.addressModeV = addressModes[1];
    info.addressModeW = addressModes[2];

    info.mipmapMode = mipmapMode;
    info.mipLodBias = lodBias;
    info.minLod = lod[0];
    info.maxLod = lod[1];

    info.anisotropyEnable = static_cast<bool>(maxAnisotropy) ? VK_TRUE : VK_FALSE;
    info.maxAnisotropy = maxAnisotropy;

    info.compareEnable = (compareOp == VK_COMPARE_OP_NEVER) ? VK_FALSE : VK_TRUE;
    info.compareOp = compareOp;

    info.borderColor = borderColor;
    info.unnormalizedCoordinates = unnormalizedCoordinates;

    return info;
}


} // namespace cth
