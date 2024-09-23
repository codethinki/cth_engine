#include "CthSampler.hpp"

#include "vulkan/base/CthCore.hpp"
#include "vulkan/resource/CthDestructionQueue.hpp"
#include "vulkan/utility/cth_vk_exceptions.hpp"


//Sampler

namespace cth::vk {

Sampler::Sampler(cth::not_null<Core const*> core, Config const& config) : _core(core) { create(config); }
Sampler::~Sampler() { optDestroy(); }
void Sampler::wrap(State const& state) {
    optDestroy();

    _handle = state.vkSampler.get();
}

void Sampler::create(Config const& config) {
    optDestroy();

    auto const createInfo = config.createInfo();

    VkSampler ptr = VK_NULL_HANDLE;
    VkResult const createResult = vkCreateSampler(_core->vkDevice(), &createInfo, nullptr, &ptr);

    CTH_STABLE_ERR(createResult != VK_SUCCESS, "failed to create sampler") {
        reset();
        throw vk::result_exception(createResult, details->exception());
    }

    _handle = ptr;
}


void Sampler::destroy() {
    DEBUG_CHECK_SAMPLER(this);

    auto const lambda = [device = _core->vkDevice(), sampler = _handle.get()] { Sampler::destroy(device, sampler); };

    auto const queue = _core->destructionQueue();
    if(queue) queue->push(lambda);
    else lambda();

    reset();
}
void Sampler::destroy(vk::not_null<VkDevice> device, VkSampler sampler) {
    CTH_WARN(sampler == VK_NULL_HANDLE, "vk_sampler should not be invalid (VK_NULL_HANDLE)") {}

    vkDestroySampler(device.get(), sampler, nullptr);
}


void Sampler::reset() { _handle = VK_NULL_HANDLE; }

#ifdef CONSTANT_DEBUG_MODE
void Sampler::debug_check(cth::not_null<Sampler*> sampler) {
    CTH_CRITICAL(!sampler->created(), "sampler must be created") {}
    DEBUG_CHECK_SAMPLER_HANDLE(sampler->get());
}
void Sampler::debug_check_handle(vk::not_null<VkSampler> sampler) {}
#endif

} // namespace cth

//Config

namespace cth::vk {
//
//Sampler::Config::Config(const VkSamplerCreateInfo& create_info) : mipmapMode(create_info.mipmapMode),
//    lodBias(create_info.mipLodBias), maxAnisotropy(create_info.maxAnisotropy), borderColor(create_info.borderColor), compareOp(create_info.compareOp),
//    unnormalizedCoordinates(create_info.unnormalizedCoordinates) {
//
//    filters[0] = create_info.minFilter;
//    filters[1] = create_info.magFilter;
//
//    addressModes[0] = create_info.addressModeU;
//    addressModes[1] = create_info.addressModeV;
//    addressModes[2] = create_info.addressModeW;
//
//
//    lod[0] = create_info.minLod;
//    lod[1] = create_info.maxLod;
//
//
//}

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
