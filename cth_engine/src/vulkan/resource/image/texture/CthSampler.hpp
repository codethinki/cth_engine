#pragma once

#include <array>
#include <volk.h>

#include "vulkan/resource/buffer/CthBaseBuffer.hpp"

//TEMP modernize

namespace cth::vk {
class Core;

class Sampler {

public:
    struct Config;
    struct State;


    explicit Sampler(cth::not_null<Core const*> core, Config const& config);
    ~Sampler();

    void wrap(State const& state);
    void create(Config const& config);

    void destroy();
    void optDestroy() { if(created()) destroy(); }

    static void destroy(vk::not_null<VkDevice> device, VkSampler sampler);

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

#ifdef CONSTANT_DEBUG_MODE
    static void debug_check(cth::not_null<Sampler*> sampler);
    static void debug_check_handle(vk::not_null<VkSampler> sampler);

#define DEBUG_CHECK_SAMPLER(sampler) Sampler::debug_check(sampler)
#define DEBUG_CHECK_SAMPLER_HANDLE(vk_sampler) Sampler::debug_check_handle(vk_sampler)
#else
#define DEBUG_CHECK_SAMPLER(sampler) ((void)0)
#define DEBUG_CHECK_SAMPLER_HANDLE(vk_sampler) ((void)0)
#endif

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

