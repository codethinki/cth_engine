#pragma once
#include "descriptor.hpp"
#include "descriptor_span.hpp"

#include "cvk/data/buffer/base_buffer.hpp"
#include "cvk/data/img/sampler_config.hpp"
#include "cvk/utility/constants.hpp"

#include <volk.h>

#include <cth/io/log.hpp>

#include <cstddef>
#include <cstdint>


namespace cvk {
class Core;

/**
 * descriptor heap of the samplers
 * @details
 * - owns a buffer with the memory and the layout of the descriptors
 * - one `DescriptorSpan`, the caller places the descriptors by index
 * - the reserved range of the driver lies at the end
 * @see `BaseCmdBuffer::bind()`
 */
class SamplerHeap {
public:
    /**
     * descriptors per type
     */
    struct Config {
        uint32_t samplers = 0;
    };

    /**
     * base constructor
     */
    SamplerHeap(Core const&, Config const&);

    /**
     * constructs and creates
     * @details calls @ref SamplerHeap(Core const&, Config const&), `create()`
     */
    explicit SamplerHeap(Core const&, Config const&, create_t);

    /**
     * @note calls `optDestroy()`
     */
    ~SamplerHeap() { optDestroy(); }

    /**
     * creates the heap buffer and its layout
     * @details layout: the samplers, the reserved range
     * @details stride: `samplerDescriptorSize` aligned to `samplerDescriptorAlignment`
     * @details reserved range: `minSamplerHeapReservedRange`
     * @pre `Core::created()`
     * @pre at least one descriptor: a heap without descriptors has no use, an app without descriptors binds no heap
     * @post `created()`
     * @note calls `optDestroy()`
     * @throws cvk::cvk_exception reason: the heap size exceeds `maxSamplerHeapSize`
     * @throws cvk::vk_result_exception result of `vmaCreateBuffer()`
     */
    void create();

    /**
     * destroys the heap buffer and forgets
     * @pre `created()`
     * @pre the gpu does not use the heap
     */
    void destroy();

    /**
     * if `created()` -> calls `destroy()`
     */
    void optDestroy() {
        if(created())
            destroy();
    }

    /**
     * encodes the sampler into sampler `idx`, then flushes its cell
     * @details clamps `SamplerConfig::maxAnisotropy` to `maxSamplerAnisotropy` of the device, warns if it clamps
     * @details flush: does nothing on host coherent memory
     * @pre `created()`
     * @pre `idx < Config::samplers`
     * @pre the gpu does not use the cell
     * @throws cvk::vk_result_exception result of `vkWriteSamplerDescriptorsEXT()`
     * @throws cvk::vk_result_exception result of `vmaFlushAllocation()`
     */
    void write(uint32_t idx, SamplerConfig const&);

    /**
     * maps a shader array of samplers at the set and binding onto the samplers
     * @details element `i` of the array -> sampler `i`
     * @tparam Descriptor `SamplerConfig`
     * @return mapping for the `descriptorMappings` of `GraphicsPipelineConfig` and `ComputePipelineConfig`
     * @pre `created()`
     * @see `DescriptorSpan::mapping()`
     */
    template<descriptor_of<SamplerHeap> Descriptor>
    [[nodiscard]] constexpr VkDescriptorSetAndBindingMappingEXT mapping(
        uint32_t set,
        uint32_t binding
    ) const;

    /**
     * maps a shader variable of samplers at the set and binding onto the sampler at an index in the push data
     * @details the index: the `uint32_t` at `pushOffset` in the push data of the draw or dispatch
     * @details an array: element `i` -> `i` samplers after it
     * @tparam Descriptor `SamplerConfig`
     * @param set of the shader variable
     * @param binding of the shader variable
     * @param pushOffset of the index in the push data, in bytes
     * @return mapping for the `descriptorMappings` of `GraphicsPipelineConfig` and `ComputePipelineConfig`
     * @pre `created()`
     * @pre `pushOffset` is a multiple of 4, `pushOffset + 4 <= maxPushDataSize`
     * @see `DescriptorSpan::pushMapping()`
     */
    template<descriptor_of<SamplerHeap> Descriptor>
    [[nodiscard]] constexpr VkDescriptorSetAndBindingMappingEXT pushMapping(
        uint32_t set,
        uint32_t binding,
        uint32_t pushOffset
    ) const;

    /**
     * @return the samplers, for the writes
     * @pre `created()`
     */
    [[nodiscard]] constexpr DescriptorSpan<SamplerConfig> samplers();

    /**
     * @return the whole heap with its reserved range
     * @pre `created()`
     */
    [[nodiscard]] VkBindHeapInfoEXT bindInfo() const;

    [[nodiscard]] constexpr bool created() const { return _buffer.created(); }
    [[nodiscard]] constexpr Core const& core() const { return _buffer.core(); }
    [[nodiscard]] constexpr Config const& config() const { return _config; }

private:
    void forget();

    BaseBuffer _buffer;
    Config _config;
    size_t _stride = 0;
    size_t _reservedOffset = 0;
    size_t _reservedSize = 0;

public:
    SamplerHeap(SamplerHeap const& other) = delete;
    SamplerHeap(SamplerHeap&& other) noexcept = default;
    SamplerHeap& operator=(SamplerHeap const& other) = delete;
    SamplerHeap& operator=(SamplerHeap&& other) noexcept = default;

    static constexpr void debug_check(SamplerHeap const&);
};
}

namespace cvk {
template<descriptor_of<SamplerHeap> Descriptor>
constexpr VkDescriptorSetAndBindingMappingEXT SamplerHeap::mapping(
    uint32_t set,
    uint32_t binding
) const {
    debug_check(*this);
    return dev::heap_mapping<Descriptor>(set, binding, 0, _stride);
}

template<descriptor_of<SamplerHeap> Descriptor>
constexpr VkDescriptorSetAndBindingMappingEXT SamplerHeap::pushMapping(
    uint32_t set,
    uint32_t binding,
    uint32_t pushOffset
) const {
    debug_check(*this);
    dev::debug_check_push_offset(core(), pushOffset);
    return dev::heap_push_mapping<Descriptor>(set, binding, 0, _stride, pushOffset);
}

constexpr DescriptorSpan<SamplerConfig> SamplerHeap::samplers() {
    debug_check(*this);
    return DescriptorSpan<SamplerConfig>{*this};
}

constexpr void SamplerHeap::debug_check(SamplerHeap const& heap) {
    CTH_CRITICAL(!heap.created(), "sampler heap must be created") {}
}
}
