#pragma once

#include <array>
#include <vulkan/vulkan.h>

#include "vulkan/resource/buffer/CthBasicBuffer.hpp"

namespace cth {
class BasicCore;

class Sampler {

public:
    struct Config;

    explicit Sampler(const BasicCore* core, const Config& config);
    ~Sampler();

private:
    void create(const Config& config);

    const BasicCore* _core;
    move_ptr<VkSampler_T> _handle = VK_NULL_HANDLE;

public:
    [[nodiscard]] VkSampler get() const { return _handle.get(); }

    Sampler(const Sampler& other) = delete;
    Sampler(Sampler&& other) = delete;
    Sampler& operator=(const Sampler& other) = delete;
    Sampler& operator=(Sampler&& other) = delete;
};
}

//Config
namespace cth {


struct Sampler::Config {
    Config() = default;
    //explicit Config(const VkSamplerCreateInfo& create_info);

    std::array<VkFilter, 2> filters{VK_FILTER_LINEAR, VK_FILTER_LINEAR}; //minFilter, magFilter
    std::array<VkSamplerAddressMode, 3> addressModes{VK_SAMPLER_ADDRESS_MODE_REPEAT}; //u, v, w

    VkSamplerMipmapMode mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    float lodBias = 0.0f;
    std::array<float, 2> lod{0.0f, VK_LOD_CLAMP_NONE}; //minLod, maxLod [VK_LOD_CLAMP_NONE => no clamp]


    float maxAnisotropy = 16; //0 => no anisotropy

    VkBorderColor borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    VkCompareOp compareOp = VK_COMPARE_OP_NEVER; //VK_COMPARE_OP_NEVER => no comparison
    VkBool32 unnormalizedCoordinates = VK_FALSE;

    [[nodiscard]] VkSamplerCreateInfo createInfo() const;

    [[nodiscard]] static Config Default();

    friend Sampler;
};

inline Sampler::Config Sampler::Config::Default() {
    Config defaultConfig;
    defaultConfig.filters = {VK_FILTER_LINEAR, VK_FILTER_LINEAR};
    defaultConfig.addressModes = {VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_REPEAT};
    defaultConfig.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    defaultConfig.lodBias = 0.0f;
    defaultConfig.lod = {0.0f, VK_LOD_CLAMP_NONE};
    defaultConfig.maxAnisotropy = 16;
    defaultConfig.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    defaultConfig.compareOp = VK_COMPARE_OP_NEVER;
    defaultConfig.unnormalizedCoordinates = VK_FALSE;
    return defaultConfig;
}

} // namespace cth
