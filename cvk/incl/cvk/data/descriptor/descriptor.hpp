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
/**
 * shader variables that a descriptor of `T` serves
 * @details image descriptors with `Sampled` 2: with and without `NonWritable`
 */
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

/**
 * maps a shader array of `T` at the set and binding onto the cells of a heap
 * @details element `i` of the array -> the cell at `offset + i * stride`
 * @param set of the shader array
 * @param binding of the shader array
 * @param offset of the first cell in bytes
 * @param stride of the cells in bytes
 * @pre `offset` and `stride` fit into 32 bits
 */
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

/**
 * checks the push offset of an index for the device
 * @details `pushOffset + 4 <= maxPushDataSize`
 */
void debug_check_push_offset(Core const&, uint32_t pushOffset);

/**
 * maps a shader variable of `T` at the set and binding onto the cell at an index in the push data
 * @details the cell at `offset + index * stride`, `index`: the `uint32_t` at `pushOffset` in the push data
 * @details an array: element `i` -> `i` cells after it
 * @param set of the shader variable
 * @param binding of the shader variable
 * @param offset of the first cell in bytes
 * @param stride of the cells in bytes
 * @param pushOffset of the index in the push data, in bytes
 * @pre `offset` and `stride` fit into 32 bits
 * @pre `pushOffset` is a multiple of 4
 */
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

/**
 * encodes the descriptor into the cell of the heap, then flushes the cell
 * @param heap buffer of the heap
 * @param offset of the cell in bytes
 * @param size of the cell in bytes
 * @pre `ImageViewConfig::debug_check()` passes for the view and the image
 * @throws cvk::vk_result_exception result of `vkWriteResourceDescriptorsEXT()`
 * @throws cvk::vk_result_exception result of `vmaFlushAllocation()`
 */
void write_descriptor(
    BaseBuffer const& heap,
    size_t offset,
    size_t size,
    SampledImageDescriptor const&
);

/**
 * @see @ref write_descriptor(BaseBuffer const&, size_t, size_t, SampledImageDescriptor const&)
 */
void write_descriptor(
    BaseBuffer const& heap,
    size_t offset,
    size_t size,
    StorageImageDescriptor const&
);

/**
 * @see @ref write_descriptor(BaseBuffer const&, size_t, size_t, SampledImageDescriptor const&)
 */
void write_descriptor(
    BaseBuffer const& heap,
    size_t offset,
    size_t size,
    InputAttachmentDescriptor const&
);

/**
 * encodes the sampler into the cell of the heap, then flushes the cell
 * @param heap buffer of the heap
 * @param offset of the cell in bytes
 * @param size of the cell in bytes
 * @details clamps `SamplerConfig::maxAnisotropy` to `maxSamplerAnisotropy` of the device, warns if it clamps
 * @throws cvk::vk_result_exception result of `vkWriteSamplerDescriptorsEXT()`
 * @throws cvk::vk_result_exception result of `vmaFlushAllocation()`
 */
void write_descriptor(BaseBuffer const& heap, size_t offset, size_t size, SamplerConfig const&);
}
