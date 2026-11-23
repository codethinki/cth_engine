#pragma once
#include "image_descriptor.hpp"

#include "cvk/data/buffer/base_buffer.hpp"
#include "cvk/data/img/sampler_config.hpp"

#include <volk.h>

#include <cth/io/log.hpp>
#include <cth/meta/concepts.hpp>

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>


namespace cvk {
class Core;
class ResourceHeap;
class SamplerHeap;

/**
 * descriptor type of a heap
 * @details `ResourceHeap`: `SampledImageDescriptor`, `StorageImageDescriptor`, `InputAttachmentDescriptor`
 * @details `SamplerHeap`: `SamplerConfig`
 */
template<class T>
concept descriptor = cth::mta::is_any_of<
    T,
    SampledImageDescriptor,
    StorageImageDescriptor,
    InputAttachmentDescriptor,
    SamplerConfig>;

/**
 * heap of the descriptor type
 * @details `type`: `SamplerHeap` for `SamplerConfig`, else `ResourceHeap`
 */
template<descriptor T>
struct heap_of {
    using type = std::conditional_t<std::same_as<T, SamplerConfig>, SamplerHeap, ResourceHeap>;
};

template<descriptor T>
using heap_of_t = typename heap_of<T>::type;

/**
 * descriptor type of the heap
 */
template<class T, class Heap>
concept descriptor_of = descriptor<T> && std::same_as<heap_of_t<T>, Heap>;
}

namespace cvk::dev {
// image descriptors with `Sampled` 2: with and without `NonWritable`
template<descriptor T>
inline constexpr VkSpirvResourceTypeFlagsEXT resource_mask = [] {
    if constexpr(std::same_as<T, SampledImageDescriptor>)
        return VK_SPIRV_RESOURCE_TYPE_SAMPLED_IMAGE_BIT_EXT;
    else if constexpr(std::same_as<T, SamplerConfig>)
        return VK_SPIRV_RESOURCE_TYPE_SAMPLER_BIT_EXT;
    else
        return VK_SPIRV_RESOURCE_TYPE_READ_ONLY_IMAGE_BIT_EXT
            | VK_SPIRV_RESOURCE_TYPE_READ_WRITE_IMAGE_BIT_EXT;
}();

template<descriptor T>
[[nodiscard]] constexpr VkDescriptorSetAndBindingMappingEXT heap_mapping(
    uint32_t set,
    uint32_t binding,
    size_t offset,
    size_t stride
) {
    constexpr auto MAX = std::numeric_limits<uint32_t>::max();
    CTH_CRITICAL(
        offset > MAX || stride > MAX,
        "offset and stride must fit into 32 bits, got {}, {}",
        offset,
        stride
    ) {}

    return VkDescriptorSetAndBindingMappingEXT{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_AND_BINDING_MAPPING_EXT,
        .pNext = nullptr,
        .descriptorSet = set,
        .firstBinding = binding,
        .bindingCount = 1,
        .resourceMask = resource_mask<T>,
        .source = VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_CONSTANT_OFFSET_EXT,
        .sourceData = {
            .constantOffset = {
                .heapOffset = static_cast<uint32_t>(offset),
                .heapArrayStride = static_cast<uint32_t>(stride),
                .pEmbeddedSampler = nullptr,
                .samplerHeapOffset = 0,
                .samplerHeapArrayStride = 0,
            },
        },
    };
}

void debug_check_push_offset(Core const&, uint32_t pushOffset);

template<descriptor T>
[[nodiscard]] constexpr VkDescriptorSetAndBindingMappingEXT heap_push_mapping(
    uint32_t set,
    uint32_t binding,
    size_t offset,
    size_t stride,
    uint32_t pushOffset
) {
    constexpr auto MAX = std::numeric_limits<uint32_t>::max();
    CTH_CRITICAL(
        offset > MAX || stride > MAX,
        "offset and stride must fit into 32 bits, got {}, {}",
        offset,
        stride
    ) {}
    CTH_CRITICAL(
        pushOffset % 4 != 0,
        "the push offset must be a multiple of 4, got {}",
        pushOffset
    ) {}

    return VkDescriptorSetAndBindingMappingEXT{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_AND_BINDING_MAPPING_EXT,
        .pNext = nullptr,
        .descriptorSet = set,
        .firstBinding = binding,
        .bindingCount = 1,
        .resourceMask = resource_mask<T>,
        .source = VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_PUSH_INDEX_EXT,
        .sourceData = {
            .pushIndex = {
                .heapOffset = static_cast<uint32_t>(offset),
                .pushOffset = pushOffset,
                .heapIndexStride = static_cast<uint32_t>(stride),
                .heapArrayStride = static_cast<uint32_t>(stride),
                .pEmbeddedSampler = nullptr,
                .useCombinedImageSamplerIndex = VK_FALSE,
                .samplerHeapOffset = 0,
                .samplerPushOffset = 0,
                .samplerHeapIndexStride = 0,
                .samplerHeapArrayStride = 0,
            },
        },
    };
}

void write_descriptor(
    BaseBuffer const& heap,
    size_t offset,
    size_t size,
    SampledImageDescriptor const&
);

void write_descriptor(
    BaseBuffer const& heap,
    size_t offset,
    size_t size,
    StorageImageDescriptor const&
);

void write_descriptor(
    BaseBuffer const& heap,
    size_t offset,
    size_t size,
    InputAttachmentDescriptor const&
);

void write_descriptor(BaseBuffer const& heap, size_t offset, size_t size, SamplerConfig const&);
}
