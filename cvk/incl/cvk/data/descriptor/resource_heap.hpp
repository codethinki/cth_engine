#pragma once
#include "descriptor.hpp"
#include "descriptor_span.hpp"
#include "image_descriptor.hpp"

#include "cvk/data/buffer/base_buffer.hpp"
#include "cvk/utility/constants.hpp"

#include <volk.h>

#include <cth/io/log.hpp>

#include <concepts>
#include <cstddef>
#include <cstdint>


namespace cvk {
class Core;

/**
 * descriptor heap of the resources
 * @details
 * - owns a buffer with the memory and the layout of the descriptors
 * - one `DescriptorSpan` per descriptor type, the caller places the descriptors by index
 * - the reserved range of the driver lies at the end
 * @see `BaseCmdBuffer::bind()`
 */
class ResourceHeap {
public:
    /**
     * descriptors per type
     */
    struct Config {
        uint32_t sampledImages = 0;
        uint32_t storageImages = 0;
        uint32_t inputAttachments = 0;
    };

    /**
     * base constructor
     */
    ResourceHeap(Core const&, Config const&);

    /**
     * constructs and creates
     * @details calls @ref ResourceHeap(Core const&, Config const&), `create()`
     */
    explicit ResourceHeap(Core const&, Config const&, create_t);

    /**
     * @note calls `optDestroy()`
     */
    ~ResourceHeap() { optDestroy(); }

    /**
     * creates the heap buffer and its layout
     * @details layout: the sampled images, the storage images, the input attachments, the reserved range
     * @details stride: `imageDescriptorSize` aligned to `imageDescriptorAlignment`
     * @details reserved range: `minResourceHeapReservedRange`
     * @pre `Core::created()`
     * @pre at least one descriptor: a heap without descriptors has no use, an app without descriptors binds no heap
     * @post `created()`
     * @note calls `optDestroy()`
     * @throws cvk::cvk_exception reason: the heap size exceeds `maxResourceHeapSize`
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
    void optDestroy() { if(created()) destroy(); }

    /**
     * encodes the descriptor into sampled image `idx`, then flushes its cell
     * @details flush: does nothing on host coherent memory
     * @pre `created()`
     * @pre `idx < Config::sampledImages`
     * @pre the gpu does not use the cell
     * @pre `ImageViewConfig::debug_check()` passes for the view and the image
     * @throws cvk::vk_result_exception result of `vkWriteResourceDescriptorsEXT()`
     * @throws cvk::vk_result_exception result of `vmaFlushAllocation()`
     */
    void write(uint32_t idx, SampledImageDescriptor const&);

    /**
     * encodes the descriptor into storage image `idx`, then flushes its cell
     * @pre `idx < Config::storageImages`
     * @pre the other preconditions of @ref write(uint32_t, SampledImageDescriptor const&)
     * @throws cvk::vk_result_exception the exceptions of @ref write(uint32_t, SampledImageDescriptor const&)
     */
    void write(uint32_t idx, StorageImageDescriptor const&);

    /**
     * encodes the descriptor into input attachment `idx`, then flushes its cell
     * @pre `idx < Config::inputAttachments`
     * @pre the other preconditions of @ref write(uint32_t, SampledImageDescriptor const&)
     * @throws cvk::vk_result_exception the exceptions of @ref write(uint32_t, SampledImageDescriptor const&)
     */
    void write(uint32_t idx, InputAttachmentDescriptor const&);

    /**
     * maps a shader array of `Descriptor` at the set and binding onto the descriptors of its type
     * @details element `i` of the array -> descriptor `i` of the type
     * @return mapping for the `descriptorMappings` of `GraphicsPipelineConfig` and `ComputePipelineConfig`
     * @pre `created()`
     * @see `DescriptorSpan::mapping()`
     */
    template<descriptor_of<ResourceHeap> Descriptor>
    [[nodiscard]] constexpr VkDescriptorSetAndBindingMappingEXT mapping(uint32_t set, uint32_t binding) const;

    /**
     * maps a shader variable of `Descriptor` at the set and binding onto the descriptor at an index in the push data
     * @details the index: the `uint32_t` at `pushOffset` in the push data of the draw or dispatch
     * @details an array: element `i` -> `i` descriptors after it
     * @param set of the shader variable
     * @param binding of the shader variable
     * @param pushOffset of the index in the push data, in bytes
     * @return mapping for the `descriptorMappings` of `GraphicsPipelineConfig` and `ComputePipelineConfig`
     * @pre `created()`
     * @pre `pushOffset` is a multiple of 4, `pushOffset + 4 <= maxPushDataSize`
     * @see `DescriptorSpan::pushMapping()`
     */
    template<descriptor_of<ResourceHeap> Descriptor>
    [[nodiscard]] constexpr VkDescriptorSetAndBindingMappingEXT pushMapping(
        uint32_t set,
        uint32_t binding,
        uint32_t pushOffset
    ) const;

    /**
     * @return the descriptors of the type, for the writes
     * @pre `created()`
     */
    [[nodiscard]] constexpr DescriptorSpan<SampledImageDescriptor> sampledImages();

    /**
     * @return the descriptors of the type, for the writes
     * @pre `created()`
     */
    [[nodiscard]] constexpr DescriptorSpan<StorageImageDescriptor> storageImages();

    /**
     * @return the descriptors of the type, for the writes
     * @pre `created()`
     */
    [[nodiscard]] constexpr DescriptorSpan<InputAttachmentDescriptor> inputAttachments();

    /**
     * @return the whole heap with its reserved range
     * @pre `created()`
     */
    [[nodiscard]] VkBindHeapInfoEXT bindInfo() const;

    [[nodiscard]] constexpr bool created() const { return _buffer.created(); }
    [[nodiscard]] constexpr Core const& core() const { return _buffer.core(); }
    [[nodiscard]] constexpr Config const& config() const { return _config; }

private:
    /**
     * @return byte offset of the first descriptor of the type
     */
    template<descriptor_of<ResourceHeap> Descriptor>
    [[nodiscard]] constexpr size_t offset() const;

    void forget();

    BaseBuffer _buffer;
    Config _config;
    size_t _stride = 0;
    size_t _reservedOffset = 0;
    size_t _reservedSize = 0;

public:
    ResourceHeap(ResourceHeap const& other) = delete;
    ResourceHeap(ResourceHeap&& other) noexcept = default;
    ResourceHeap& operator=(ResourceHeap const& other) = delete;
    ResourceHeap& operator=(ResourceHeap&& other) noexcept = default;

    static constexpr void debug_check(ResourceHeap const&);
};
}

namespace cvk {
template<descriptor_of<ResourceHeap> Descriptor>
constexpr VkDescriptorSetAndBindingMappingEXT ResourceHeap::mapping(uint32_t set, uint32_t binding) const {
    debug_check(*this);
    return dev::heap_mapping<Descriptor>(set, binding, offset<Descriptor>(), _stride);
}

template<descriptor_of<ResourceHeap> Descriptor>
constexpr VkDescriptorSetAndBindingMappingEXT ResourceHeap::pushMapping(
    uint32_t set,
    uint32_t binding,
    uint32_t pushOffset
) const {
    debug_check(*this);
    dev::debug_check_push_offset(core(), pushOffset);
    return dev::heap_push_mapping<Descriptor>(set, binding, offset<Descriptor>(), _stride, pushOffset);
}

constexpr DescriptorSpan<SampledImageDescriptor> ResourceHeap::sampledImages() {
    debug_check(*this);
    return DescriptorSpan<SampledImageDescriptor>{*this};
}

constexpr DescriptorSpan<StorageImageDescriptor> ResourceHeap::storageImages() {
    debug_check(*this);
    return DescriptorSpan<StorageImageDescriptor>{*this};
}

constexpr DescriptorSpan<InputAttachmentDescriptor> ResourceHeap::inputAttachments() {
    debug_check(*this);
    return DescriptorSpan<InputAttachmentDescriptor>{*this};
}

template<descriptor_of<ResourceHeap> Descriptor>
constexpr size_t ResourceHeap::offset() const {
    if constexpr(std::same_as<Descriptor, SampledImageDescriptor>) return 0;
    else if constexpr(std::same_as<Descriptor, StorageImageDescriptor>) return size_t{_config.sampledImages} * _stride;
    else return (size_t{_config.sampledImages} + _config.storageImages) * _stride;
}

constexpr void ResourceHeap::debug_check(ResourceHeap const& heap) {
    CTH_CRITICAL(!heap.created(), "resource heap must be created") {}
}
}
