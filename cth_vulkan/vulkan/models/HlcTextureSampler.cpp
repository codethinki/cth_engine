#include "HlcTextureSampler.hpp"

#include "..\core\CthDevice.hpp"

#include <stdexcept>

namespace cth {
	using namespace std;
	/**@note
	 *@param filter filter to apply when having more fragments than texels
	 *@param address_mode how to handle out of image texel positions
	 * anisotropic filtering helps with sharp angles on views
	 */
	HlcTextureSampler::HlcTextureSampler(Device& device, const VkFilter filter, const VkSamplerAddressMode address_mode) : device{device} {
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;

		samplerInfo.unnormalizedCoordinates = VK_FALSE;

		samplerInfo.magFilter = filter; //magnification
		samplerInfo.minFilter = filter; //minimization

		samplerInfo.addressModeU = address_mode; //X
		samplerInfo.addressModeV = address_mode; //Y
		samplerInfo.addressModeW = address_mode; //Z
		samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;

		samplerInfo.anisotropyEnable = VK_TRUE;
		samplerInfo.maxAnisotropy = device.physicalProperties.limits.maxSamplerAnisotropy;

		samplerInfo.compareEnable = VK_FALSE;
		samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerInfo.mipLodBias = 0.f;
		samplerInfo.minLod = 0.f;
		samplerInfo.maxLod = 0.f;

		if(vkCreateSampler(device.device(), &samplerInfo, nullptr, &textureSampler) != VK_SUCCESS) throw runtime_error("HlcTextureSampler: failed to create texture sampler");
}
	HlcTextureSampler::~HlcTextureSampler() {
		vkDestroySampler(device.device(), textureSampler, nullptr);	
	}

}
