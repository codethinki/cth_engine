#pragma once

#include "src/vulkan/base/CthDeviceTable.hpp"

#include <array>
#include <volk.h>

#include "src/vulkan/resource/buffer/CthBaseBuffer.hpp"

//TEMP modernize

namespace cth::vk {
class Core;

class Sampler {

public:
    struct Config;
    struct State;


    explicit Sampler(Core const& core, Config const& config);
    ~Sampler();

    void wrap(State const& state);
    void create(Config const& config);

    void destroy();
    void optDestroy() { if(created()) destroy(); }

    static void destroy(DeviceTable table, VkSampler sampler);

private:
    void reset();

    cth::not_null<Core const*> _core;

    move_ptr<VkSampler_T> _handle = VK_NULL_HANDLE;

public:
    [[nodiscard]] VkSampler get() const { return _handle.get(); }
    [[nodiscard]] bool created() const { return _handle != VK_NULL_HANDLE; }


    Sampler(Sampler const& other) = delete;
    Sampler& operator=(Sampler const& other) = delete;
    Sampler(Sampler&& other) noexcept = default;
    Sampler& operator=(Sampler&& other) noexcept = default;

    static void debug_check(Sampler const& sampler);
    static void debug_check_handle(vk::not_null<VkSampler> sampler);
};
}

//Config

namespace cth::vk {
struct Sampler::Config {
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

    friend Sampler;
};
} // namespace cth

//State

namespace cth::vk {
struct Sampler::State {
    vk::not_null<VkSampler> vkSampler;
};

}

//debug checks

namespace cth::vk {

inline void Sampler::debug_check(Sampler const& sampler) {
    CTH_CRITICAL(!sampler.created(), "sampler must be created") {}
    Sampler::debug_check_handle(sampler.get());
}
inline void Sampler::debug_check_handle(vk::not_null<VkSampler> sampler) {}

}
