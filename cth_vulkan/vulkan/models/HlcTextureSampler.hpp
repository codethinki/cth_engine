#pragma once

#include <vulkan/vulkan.h>


namespace cth {
class Device;

class HlcTextureSampler {
public:
	HlcTextureSampler(Device& device, VkFilter filter, VkSamplerAddressMode address_mode);
	~HlcTextureSampler();

	[[nodiscard]] VkSampler sampler() const { return textureSampler; }
	
	HlcTextureSampler(const HlcTextureSampler&) = delete;
	HlcTextureSampler& operator=(const HlcTextureSampler&) = delete;

private:
	Device& device;
	VkSampler textureSampler;
	VkSamplerCreateInfo samplerInfo{};

};
}
