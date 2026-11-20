#pragma once
#include "descriptor.hpp"

#include <volk.h>

#include <cth/io/log.hpp>
#include <cth/ptr/ref_wrapper.hpp>

#include <concepts>
#include <cstdint>


namespace cvk {
/**
 * descriptors of one type in a heap, an array of the shader
 * @details
 * - non-owning view of the heap, the heap owns the memory and the layout
 * - write-only, the driver encodes the cells
 * @tparam Descriptor type of the descriptors
 * @see `ResourceHeap`, `SamplerHeap`
 */
template<descriptor Descriptor>
class DescriptorSpan {
public:
    class reference;

    /**
     * views the descriptors of `Descriptor` in the heap
     * @see `ResourceHeap::sampledImages()`, `ResourceHeap::storageImages()`, `ResourceHeap::inputAttachments()`,
     *  `SamplerHeap::samplers()`
     */
    constexpr explicit DescriptorSpan(heap_of_t<Descriptor> const& heap) : _heap{heap} {}

    /**
     * @return write-only reference to the cell
     * @pre `idx < size()`
     */
    [[nodiscard]] constexpr reference operator[](uint32_t idx) const;

    /**
     * @return the descriptors of `Descriptor` in the config of the heap
     */
    [[nodiscard]] constexpr uint32_t size() const;

    /**
     * maps a shader array of `Descriptor` at the set and binding onto the span
     * @details element `i` of the array -> cell `i` of the span
     * @return mapping for the `descriptorMappings` of `GraphicsPipelineConfig` and `ComputePipelineConfig`
     * @see `ResourceHeap::mapping()`, `SamplerHeap::mapping()`
     */
    [[nodiscard]] constexpr VkDescriptorSetAndBindingMappingEXT mapping(uint32_t set, uint32_t binding) const;

private:
    cth::ref_wrapper<heap_of_t<Descriptor> const> _heap;
};

/**
 * write-only reference to a cell of a `DescriptorSpan`
 * @details copy assignment between references is deleted: a cell cannot be read
 */
template<descriptor Descriptor>
class DescriptorSpan<Descriptor>::reference {
public:
    /**
     * references the cell of the span
     * @pre `idx < span.size()`
     */
    constexpr reference(DescriptorSpan const& span, uint32_t idx) : _span{span}, _idx{idx} {}

    /**
     * encodes the descriptor into the cell
     * @details calls `ResourceHeap::write()` or `SamplerHeap::write()`
     * @pre the preconditions of the `write()` of the heap
     * @throws cvk::vk_result_exception the exceptions of the `write()` of the heap
     */
    reference const& operator=(Descriptor const& descriptor) const;

private:
    DescriptorSpan _span; ///< a copy: the reference outlives a temporary span
    uint32_t _idx;

public:
    reference(reference const& other) = default;
    reference(reference&& other) noexcept = default;
    reference& operator=(reference const& other) = delete;
    reference& operator=(reference&& other) = delete;
};
}

namespace cvk {
// the definitions reach the heap through the dependent `heap_of_t<Descriptor>`: the lookup runs at the
// instantiation, the heap headers define the heaps before
template<descriptor Descriptor>
constexpr auto DescriptorSpan<Descriptor>::operator[](uint32_t idx) const -> reference {
    CTH_CRITICAL(idx >= size(), "index must lie in the span, got {} of {}", idx, size()) {}
    return reference{*this, idx};
}

template<descriptor Descriptor>
constexpr uint32_t DescriptorSpan<Descriptor>::size() const {
    auto const& config = _heap->config();
    if constexpr(std::same_as<Descriptor, SampledImageDescriptor>) return config.sampledImages;
    else if constexpr(std::same_as<Descriptor, StorageImageDescriptor>) return config.storageImages;
    else if constexpr(std::same_as<Descriptor, InputAttachmentDescriptor>) return config.inputAttachments;
    else return config.samplers;
}

template<descriptor Descriptor>
constexpr VkDescriptorSetAndBindingMappingEXT DescriptorSpan<Descriptor>::mapping(
    uint32_t set,
    uint32_t binding
) const {
    return _heap->template mapping<Descriptor>(set, binding);
}

template<descriptor Descriptor>
auto DescriptorSpan<Descriptor>::reference::operator=(Descriptor const& descriptor) const -> reference const& {
    _span._heap->write(_idx, descriptor);
    return *this;
}
}
