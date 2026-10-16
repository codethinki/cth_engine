#include "cvk/data/img/sampler.hpp"

#include "cvk/base/core.hpp"
#include "cvk/base/physical_device.hpp"
#include "cvk/utility/vk_exceptions.hpp"

#include <algorithm>


namespace cvk {
Sampler::Sampler(Core const& core, Config const& config) : _handle{core}, _config{config} {
    Core::debug_check(core);
    _config.maxAnisotropy = std::min(_config.maxAnisotropy, core.physicalDevice().limits().maxSamplerAnisotropy);
}

Sampler::Sampler(Core const& core, Config const& config, State const& state) : Sampler{core, config} { wrap(state); }

Sampler::Sampler(Core const& core, Config const& config, create_t) : Sampler{core, config} { create(); }

void Sampler::wrap(State const& state) {
    optDestroy();

    _handle.reset(state.vkSampler.get(), core());
}

void Sampler::create() {
    optDestroy();

    auto const info = _config.createInfo();

    VkSampler ptr = VK_NULL_HANDLE;
    auto const result = core().functions()->vkCreateSampler(core().vkDevice(), &info, nullptr, &ptr);

    CTH_STABLE_ERR(result != VK_SUCCESS, "failed to create sampler") {
        forget();
        throw cvk::vk_result_exception{result, details->exception()};
    }

    _handle.reset(ptr, core());
}

void Sampler::destroy() {
    debug_check(*this);

    _handle.reset();

    forget();
}

Sampler::State Sampler::release() {
    debug_check(*this);

    State const state{_handle.release()};
    forget();
    return state;
}

void Sampler::destroy(DeviceTable table, VkSampler vkSampler) {
    CTH_WARN(vkSampler == VK_NULL_HANDLE, "vkSampler invalid (VK_NULL_HANDLE)") {}

    table->vkDestroySampler(table.device(), vkSampler, nullptr);
}

void Sampler::forget() {}
}
