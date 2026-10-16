#include "cvk/data/img/sampler.hpp"

#include "cvk/base/core.hpp"
#include "cvk/base/physical_device.hpp"
#include "cvk/utility/vk_exceptions.hpp"

#include <algorithm>


namespace cvk {
Sampler::Sampler(Core const& core, Config const& config) : _deviceTable{core.deviceTable()}, _config{config} {
    _config.maxAnisotropy = std::min(_config.maxAnisotropy, core.physicalDevice().limits().maxSamplerAnisotropy);
}

Sampler::Sampler(Core const& core, Config const& config, State const& state) : Sampler{core, config} { wrap(state); }

Sampler::Sampler(Core const& core, Config const& config, create_t) : Sampler{core, config} { create(); }

void Sampler::wrap(State const& state) {
    optDestroy();

    _handle = state.vkSampler.get();
}

void Sampler::create() {
    optDestroy();

    auto const info = _config.createInfo();

    VkSampler ptr = VK_NULL_HANDLE;
    auto const result = _deviceTable->vkCreateSampler(_deviceTable.device(), &info, nullptr, &ptr);

    CTH_STABLE_ERR(result != VK_SUCCESS, "failed to create sampler") {
        reset();
        throw cvk::vk_result_exception{result, details->exception()};
    }

    _handle = ptr;
}

void Sampler::destroy() {
    debug_check(*this);

    destroy(_deviceTable, _handle.get());

    reset();
}

Sampler::State Sampler::release() {
    debug_check(*this);

    State const state{_handle.get()};
    reset();
    return state;
}

void Sampler::destroy(DeviceTable table, VkSampler vkSampler) {
    CTH_WARN(vkSampler == VK_NULL_HANDLE, "vkSampler invalid (VK_NULL_HANDLE)") {}

    table->vkDestroySampler(table.device(), vkSampler, nullptr);
}

void Sampler::reset() { _handle = VK_NULL_HANDLE; }
}
