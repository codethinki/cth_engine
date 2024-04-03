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
    const auto createInfo = config.info();

    const VkResult createResult = vkCreateSampler(device->get(), &createInfo, nullptr, &vkSampler);

    CTH_STABLE_ERR(createResult != VK_SUCCESS, "failed to create sampler")
        throw except::vk_result_exception(createResult, details->exception());

    cth::log::msg("created sampler");
}



} // namespace cth

//Config

namespace cth {

void Sampler::Config::setDefault(Config& config) {
    auto& vkConfig = config.vkInfo;

    vkConfig.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    vkConfig.magFilter = VK_FILTER_LINEAR;
    vkConfig.minFilter = VK_FILTER_LINEAR;
    vkConfig.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    vkConfig.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    vkConfig.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    vkConfig.anisotropyEnable = VK_TRUE;
    vkConfig.maxAnisotropy = 16;
    vkConfig.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    vkConfig.unnormalizedCoordinates = VK_FALSE;
    vkConfig.compareEnable = VK_FALSE;
    vkConfig.compareOp = VK_COMPARE_OP_ALWAYS;
    vkConfig.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    vkConfig.mipLodBias = 0.f;
    vkConfig.minLod = 0.f;
    vkConfig.maxLod = 1000.f;
}

} // namespace cth
