#include "cvk/data/descriptor/resource_heap.hpp"

#include "cvk/base/core.hpp"
#include "cvk/base/physical_device.hpp"
#include "cvk/data/descriptor/heap_buffer.hpp"

#include <cth/numeric.hpp>

#include <algorithm>


namespace cvk {
ResourceHeap::ResourceHeap(Core const& core, Config const& config) : _buffer{core, dev::HEAP_BUFFER_CONFIG},
    _config{config} {}

ResourceHeap::ResourceHeap(Core const& core, Config const& config, create_t) : ResourceHeap{core, config} {
    create();
}

void ResourceHeap::create() {
    Core::debug_check(core());
    auto const [sampledImages, storageImages, inputAttachments] = _config;
    CTH_CRITICAL(
        sampledImages + storageImages + inputAttachments == 0,
        "resource heap must have at least one descriptor"
    ) {}

    optDestroy();

    auto const& properties = core().physicalDevice().descriptorHeapProperties();

    auto const stride = cth::num::align_to(properties.imageDescriptorSize, properties.imageDescriptorAlignment);
    size_t const descriptors = size_t{sampledImages} + storageImages + inputAttachments;
    auto const reservedOffset = cth::num::align_to(
        descriptors * stride,
        std::max(properties.bufferDescriptorAlignment, properties.imageDescriptorAlignment)
    );
    auto const reservedSize = properties.minResourceHeapReservedRange;

    dev::create_heap_buffer(
        _buffer,
        reservedOffset + reservedSize,
        properties.resourceHeapAlignment,
        properties.maxResourceHeapSize,
        "resource"
    );

    _stride = stride;
    _reservedOffset = reservedOffset;
    _reservedSize = reservedSize;
}

void ResourceHeap::destroy() {
    debug_check(*this);

    _buffer.destroy();

    forget();
}

void ResourceHeap::write(uint32_t idx, SampledImageDescriptor const& descriptor) const {
    debug_check(*this);
    CTH_CRITICAL(
        idx >= _config.sampledImages,
        "index must lie in the sampled images, got {} of {}",
        idx,
        _config.sampledImages
    ) {}
    dev::write_descriptor(_buffer, offset<SampledImageDescriptor>() + idx * _stride, _stride, descriptor);
}

void ResourceHeap::write(uint32_t idx, StorageImageDescriptor const& descriptor) const {
    debug_check(*this);
    CTH_CRITICAL(
        idx >= _config.storageImages,
        "index must lie in the storage images, got {} of {}",
        idx,
        _config.storageImages
    ) {}
    dev::write_descriptor(_buffer, offset<StorageImageDescriptor>() + idx * _stride, _stride, descriptor);
}

void ResourceHeap::write(uint32_t idx, InputAttachmentDescriptor const& descriptor) const {
    debug_check(*this);
    CTH_CRITICAL(
        idx >= _config.inputAttachments,
        "index must lie in the input attachments, got {} of {}",
        idx,
        _config.inputAttachments
    ) {}
    dev::write_descriptor(_buffer, offset<InputAttachmentDescriptor>() + idx * _stride, _stride, descriptor);
}

VkBindHeapInfoEXT ResourceHeap::bindInfo() const {
    debug_check(*this);
    return dev::heap_bind_info(_buffer, _reservedOffset, _reservedSize);
}

void ResourceHeap::forget() {
    _stride = 0;
    _reservedOffset = 0;
    _reservedSize = 0;
}
}
