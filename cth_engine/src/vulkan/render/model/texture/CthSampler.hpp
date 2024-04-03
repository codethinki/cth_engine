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

    struct Config {
        Config() = default;
        Config(const VkSamplerCreateInfo& create_info) : vkInfo(create_info) {}

        static void setDefault(Config& config);

        VkSamplerCreateInfo vkInfo;

        [[nodiscard]] VkSamplerCreateInfo info() const { return vkInfo; }
        friend Sampler;
    };

    Sampler(const Sampler& other) = delete;
    Sampler(Sampler&& other) = delete;
    Sampler& operator=(const Sampler& other) = delete;
    Sampler& operator=(Sampler&& other) = delete;
};
}
