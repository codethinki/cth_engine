#include "jvk/data/sampler_heap.hpp"

#include "jvk/heap_bindings.hpp"
#include "jvk/utility/exception.hpp"

#include "cvk/base/core.hpp"

#include <exception>


namespace jvk {
SamplerHeap::SamplerHeap(cvk::Core const& core, Config const& config) :
    _vk{core, config},
    _cells{config.samplers},
    _slots(config.samplers) {}

SamplerHeap::SamplerHeap(cvk::Core const& core, Config const& config, cvk::create_t) :
    SamplerHeap{core, config} { create(); }

void SamplerHeap::create() {
    optDestroy();

    _vk.create();
}

void SamplerHeap::destroy() {
    cvk::SamplerHeap::debug_check(_vk);
    CTH_CRITICAL(
        _cells.used() != 0,
        "every sampler of the heap must be gone, got {}",
        _cells.used()
    ) {}

    _vk.destroy();

    forget();
}

auto SamplerHeap::insert(cvk::SamplerConfig const& config) const -> Sampler {
    std::scoped_lock const lock{*_mutexPtr};

    if(auto const it = _indices.find(config); it != _indices.end()) {
        ++_slots[it->second].refs;
        return Sampler{*this, it->second};
    }

    auto const idxOpt = _cells.insert();
    JVK_STABLE_THROW(
        !idxOpt.has_value(),
        "every sampler cell is in use, got {} cells",
        _cells.size()
    ) {}

    try {
        _vk.write(*idxOpt, config);
        _indices.emplace(config, *idxOpt);
    }
    catch(...) {
        _cells.erase(*idxOpt);
        throw;
    }
    _slots[*idxOpt] = slot{.refs = 1, .config = config};
    return Sampler{*this, *idxOpt};
}

std::vector<VkDescriptorSetAndBindingMappingEXT> SamplerHeap::mappings() const {
    return {_vk.mapping<cvk::SamplerConfig>(constants::HEAP_SET, constants::HEAP_SAMPLER_BINDING)};
}

void SamplerHeap::ref(uint32_t idx) const {
    std::scoped_lock const lock{*_mutexPtr};
    CTH_CRITICAL(_slots[idx].refs == 0, "the cell {} must be in use", idx) {}
    ++_slots[idx].refs;
}

void SamplerHeap::unref(uint32_t idx) const {
    std::scoped_lock const lock{*_mutexPtr};
    auto& [refs, config] = _slots[idx];
    CTH_CRITICAL(refs == 0, "the cell {} must be in use", idx) {}

    if(--refs != 0)
        return;
    _indices.erase(config);
    _cells.erase(idx);
}

void SamplerHeap::forget() {
    auto const& config = _vk.config();
    _cells = dev::free_list{config.samplers};
    _indices.clear();
    _slots.assign(config.samplers, slot{});
}
}
