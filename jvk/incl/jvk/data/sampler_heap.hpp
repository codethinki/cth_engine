#pragma once
#include "free_list.hpp"
#include "heap_idx.hpp"

#include "cvk/data/descriptor/sampler_heap.hpp"
#include "cvk/data/img/sampler_config.hpp"

#include <volk.h>

#include <cth/data/hash_map.hpp>
#include <cth/io/log.hpp>
#include <cth/ptr/move_ptr.hpp>
#include <cth/ptr/ref_wrapper.hpp>

#include <cstdint>
#include <mutex>
#include <tuple>
#include <utility>
#include <vector>


namespace cvk {
class Core;
}

namespace jvk {
/**
 * descriptor heap of the samplers, places the descriptors
 * @details
 * - equal configs share one cell, like the keys of a `std::set`
 * - `insert()` returns a counted handle of the cell, the last handle returns the cell
 * - `Stage::retain()` keeps a handle while the gpu uses it
 * - the shader convention: `mappings()` and `jvk/heaps.glsl`
 * - owns a `cvk::SamplerHeap`, `vk()` reads it
 * - thread safe
 */
class SamplerHeap {
public:
    using Config = cvk::SamplerHeap::Config;

    struct Cell;
    class Sampler;

    /**
     * creates the cvk heap
     * @pre `cvk::Core::created()`
     * @pre the preconditions of `cvk::SamplerHeap::create()`
     * @throws cvk::cvk_exception the exceptions of `cvk::SamplerHeap::create()`
     */
    SamplerHeap(cvk::Core const&, Config const&);

    /**
     * @pre no handle and no cell in a stasis of the heap is left
     */
    ~SamplerHeap();

    /**
     * the cell of the config: an equal config -> its cell, else a free cell with the config
     * @return a counted handle of the cell
     * @throws jvk::jvk_exception reason: every cell is in use
     * @throws cvk::vk_result_exception the exceptions of `cvk::SamplerHeap::write()`
     */
    [[nodiscard]] Sampler insert(cvk::SamplerConfig const&) const;

    /**
     * @return the mappings of the shader convention: the samplers
     * @details at `constants::HEAP_SET`, `constants::HEAP_SAMPLER_BINDING`
     * @see `jvk/heaps.glsl`, `ResourceHeap::mappings()`
     */
    [[nodiscard]] std::vector<VkDescriptorSetAndBindingMappingEXT> mappings() const;

    /**
     * the cvk heap, for binds and inheritances
     * @details const: no write of a cvk descriptor bypasses the cells
     */
    [[nodiscard]] constexpr cvk::SamplerHeap const& vk() const { return _vk; }

    [[nodiscard]] constexpr Config const& config() const { return _vk.config(); }

private:
    /**
     * adds a handle of the cell
     */
    void ref(uint32_t idx) const;

    /**
     * removes a handle of the cell, the last one returns the cell
     */
    void unref(uint32_t idx) const;

    /**
     * the handles and the config of a cell
     */
    struct slot {
        uint32_t refs = 0;
        cvk::SamplerConfig config{};
    };

    mutable std::mutex _mutex{};
    mutable cvk::SamplerHeap _vk; ///< written under `_mutex`
    mutable dev::free_list _cells; ///< under `_mutex`
    mutable cth::dt::flat_hash_map<cvk::SamplerConfig, uint32_t> _indices{}; ///< under `_mutex`
    mutable std::vector<slot> _slots; ///< one per cell, under `_mutex`

public:
    SamplerHeap(SamplerHeap const& other) = delete;
    SamplerHeap(SamplerHeap&& other) = delete;
    SamplerHeap& operator=(SamplerHeap const& other) = delete;
    SamplerHeap& operator=(SamplerHeap&& other) = delete;
};
}

namespace jvk {
/**
 * handle of a sampler cell, for `Stasis`
 */
struct SamplerHeap::Cell {
    cth::ref_wrapper<SamplerHeap const> heap;
    uint32_t idx;

    /**
     * removes the handle, the last one returns the cell
     * @pre the gpu does not use the cell
     */
    void erase() const { heap->unref(idx); }
};

/**
 * counted handle of a sampler cell
 * @details
 * - a copy is one more handle of the cell, the last destructor returns the cell
 * - release a handle with `Stage::retain()` while the gpu uses it
 * - a moved-from handle has no cell
 */
class SamplerHeap::Sampler {
public:
    struct State;

    /**
     * takes the counted handle of the cell
     * @details `SamplerHeap::insert()` constructs it
     * @pre the heap counted the handle for this object
     */
    Sampler(SamplerHeap const& heap, uint32_t idx) : _heapPtr{&heap}, _idx{idx} {}

    ~Sampler() {
        if(_heapPtr) _heapPtr->unref(_idx);
    }

    /**
     * gives the handle to the state, does not remove it
     * @pre the object has a cell
     */
    [[nodiscard]] State release();

    /**
     * @return index of the cell, for the push data
     */
    [[nodiscard]] constexpr heap_idx<Sampler> idx() const { return heap_idx<Sampler>{_idx}; }

private:
    cth::move_ptr<SamplerHeap const> _heapPtr;
    uint32_t _idx;

public:
    Sampler(Sampler const& other) : _heapPtr{other._heapPtr}, _idx{other._idx} {
        if(_heapPtr) _heapPtr->ref(_idx);
    }

    Sampler(Sampler&& other) noexcept = default;

    /**
     * copy and move assignment: the old handle goes, the new one comes
     */
    Sampler& operator=(Sampler other) noexcept {
        std::swap(_heapPtr, other._heapPtr);
        std::swap(_idx, other._idx);
        return *this;
    }
};

struct SamplerHeap::Sampler::State {
    Cell cell;

    /**
     * the handle to remove
     */
    [[nodiscard]] constexpr std::tuple<Cell> decompose() && { return {cell}; }
};

inline auto SamplerHeap::Sampler::release() -> State {
    CTH_CRITICAL(!_heapPtr, "the sampler must have a cell") {}
    return State{.cell = {.heap = *_heapPtr.release(), .idx = _idx}};
}
}

namespace jvk {
using Sampler = SamplerHeap::Sampler;
}
