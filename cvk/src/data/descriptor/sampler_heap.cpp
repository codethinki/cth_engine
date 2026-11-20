#include "cvk/data/descriptor/sampler_heap.hpp"

#include "cvk/base/core.hpp"
#include "cvk/base/physical_device.hpp"
#include "cvk/data/descriptor/heap_buffer.hpp"


namespace cvk {
SamplerHeap::SamplerHeap(Core const& core, Config const& config) : _buffer{core, dev::HEAP_BUFFER_CONFIG},
    _config{config} {}

SamplerHeap::SamplerHeap(Core const& core, Config const& config, create_t) : SamplerHeap{core, config} { create(); }

void SamplerHeap::create() {
    Core::debug_check(core());
    CTH_CRITICAL(_config.samplers == 0, "sampler heap must have at least one descriptor") {}

    optDestroy();

    auto const& properties = core().physicalDevice().descriptorHeapProperties();

    auto const stride = dev::align_up(properties.samplerDescriptorSize, properties.samplerDescriptorAlignment);
    auto const reservedOffset = dev::align_up(_config.samplers * stride, properties.samplerDescriptorAlignment);
    auto const reservedSize = properties.minSamplerHeapReservedRange;

    dev::create_heap_buffer(
        _buffer,
        reservedOffset + reservedSize,
        properties.samplerHeapAlignment,
        properties.maxSamplerHeapSize,
        "sampler"
    );

    _stride = stride;
    _reservedOffset = reservedOffset;
    _reservedSize = reservedSize;
}

void SamplerHeap::destroy() {
    debug_check(*this);

    _buffer.destroy();

    forget();
}

void SamplerHeap::write(uint32_t idx, SamplerConfig const& sampler) const {
    debug_check(*this);
    CTH_CRITICAL(idx >= _config.samplers, "index must lie in the samplers, got {} of {}", idx, _config.samplers) {}
    dev::write_descriptor(_buffer, idx * _stride, _stride, sampler);
}

VkBindHeapInfoEXT SamplerHeap::bindInfo() const {
    debug_check(*this);
    return dev::heap_bind_info(_buffer, _reservedOffset, _reservedSize);
}

void SamplerHeap::forget() {
    _stride = 0;
    _reservedOffset = 0;
    _reservedSize = 0;
}
}
