#include "jvk/res/img/texture/sampler.hpp"

#include "jvk/base/core.hpp"
#include "jvk/res/destruction_queue.hpp"
#include "jvk/utility/vk_exceptions.hpp"


//Sampler

namespace jvk {

Sampler::Sampler(Core const& core, Config const& config) : _core{&core} { create(config); }
Sampler::~Sampler() { optDestroy(); }

void Sampler::wrap(State const& state) {
    optDestroy();

    _handle = state.vkSampler.get();
}

void Sampler::create(Config const& config) {
    optDestroy();

    auto const createInfo = config.createInfo();

    VkSampler ptr = VK_NULL_HANDLE;
    VkResult const createResult = _core->functions()->vkCreateSampler(_core->vkDevice(), &createInfo, nullptr,
        &ptr);

    CTH_STABLE_ERR(createResult != VK_SUCCESS, "failed to create sampler") {
        reset();
        throw jvk::result_exception(createResult, details->exception());
    }

    _handle = ptr;
}


void Sampler::destroy() {
    Sampler::debug_check(*this);

    auto const lambda = [table = _core->deviceTable(), sampler = _handle.get()] {
        Sampler::destroy(table, sampler);
    };

    auto const queue = _core->destructionQueue();
    if(queue) queue->push(lambda);
    else lambda();

    reset();
}

void Sampler::destroy(DeviceTable table, VkSampler sampler) {
    CTH_WARN(sampler == VK_NULL_HANDLE, "vk_sampler should not be invalid (VK_NULL_HANDLE)") {}

    table->vkDestroySampler(table.device(), sampler, nullptr);
}


void Sampler::reset() { _handle = VK_NULL_HANDLE; }

} // namespace cth

//Config

namespace jvk {
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