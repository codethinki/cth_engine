#pragma once

#include <array>
#include <vulkan/vulkan.h>

namespace cth {
using namespace std;

class Device;

class Sampler {

public:
    struct Config;

    explicit Sampler(Device* device, const Config& config);
    ~Sampler();

private:
    void create(const Config& config);

    Device* device;
    VkSampler vkSampler = VK_NULL_HANDLE;

public:
    [[nodiscard]] VkSampler get() const { return vkSampler; }

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
    explicit Config(const VkSamplerCreateInfo& create_info);


    array<VkFilter, 2> filters{VK_FILTER_LINEAR, VK_FILTER_LINEAR}; //minFilter, magFilter
    array<VkSamplerAddressMode, 3> addressModes{VK_SAMPLER_ADDRESS_MODE_REPEAT}; //u, v, w

    VkSamplerMipmapMode mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    float lodBias = 0.0f;
    array<float, 2> lod{0.0f, VK_LOD_CLAMP_NONE}; //minLod, maxLod [VK_LOD_CLAMP_NONE => no clamp]


    float maxAnisotropy = 16; //0 => no anisotropy

    VkBorderColor borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    VkCompareOp compareOp = VK_COMPARE_OP_NEVER; //VK_COMPARE_OP_NEVER => no comparison
    VkBool32 unnormalizedCoordinates = VK_FALSE;

    [[nodiscard]] VkSamplerCreateInfo createInfo() const;
    friend Sampler;
};


}