#include "jvk/data/resource_heap.hpp"

#include "jvk/heap_bindings.hpp"
#include "jvk/utility/exception.hpp"

#include "cvk/base/core.hpp"
#include "cvk/utility/format.hpp"

#include <concepts>
#include <exception>


namespace jvk {
ResourceHeap::ResourceHeap(cvk::Core const& core, Config const& config) :
    _vk{core, config},
    _sampledImages{config.sampledImages},
    _storageImages{config.storageImages},
    _inputAttachments{config.inputAttachments} {}

ResourceHeap::ResourceHeap(cvk::Core const& core, Config const& config, cvk::create_t) :
    ResourceHeap{core, config} { create(); }

void ResourceHeap::create() {
    optDestroy();

    _vk.create();
}

void ResourceHeap::destroy() {
    cvk::ResourceHeap::debug_check(_vk);
    CTH_CRITICAL(
        _sampledImages.used() + _storageImages.used() + _inputAttachments.used() != 0,
        "every handle of the heap must be gone, got {} sampled images, {} storage images, {} input attachments",
        _sampledImages.used(),
        _storageImages.used(),
        _inputAttachments.used()
    ) {}

    _vk.destroy();

    forget();
}

auto ResourceHeap::insert(cvk::SampledImageDescriptor const& descriptor) const -> SampledImage {
    return insertImage(descriptor);
}

auto ResourceHeap::insert(cvk::StorageImageDescriptor const& descriptor) const -> StorageImage {
    return insertImage(descriptor);
}

auto ResourceHeap::insert(
    cvk::InputAttachmentDescriptor const& descriptor
) const -> InputAttachment { return insertImage(descriptor); }

std::vector<VkDescriptorSetAndBindingMappingEXT> ResourceHeap::mappings() const {
    return {
        _vk.mapping<cvk::SampledImageDescriptor>(
            constants::HEAP_SET,
            constants::HEAP_SAMPLED_IMAGE_BINDING
        ),
        _vk.mapping<cvk::StorageImageDescriptor>(
            constants::HEAP_SET,
            constants::HEAP_STORAGE_IMAGE_BINDING
        ),
    };
}

VkDescriptorSetAndBindingMappingEXT ResourceHeap::pushMapping(
    uint32_t set,
    uint32_t binding,
    uint32_t pushOffset
) const { return _vk.pushMapping<cvk::InputAttachmentDescriptor>(set, binding, pushOffset); }

template<cvk::descriptor_of<cvk::ResourceHeap> Descriptor>
auto ResourceHeap::insertImage(Descriptor const& descriptor) const -> ImageHandle<Descriptor> {
    CTH_CRITICAL(
        descriptor.layout != ImageHandle<Descriptor>::LAYOUT,
        "the descriptor must have the layout of its handle, got {} for {}",
        descriptor.layout,
        ImageHandle<Descriptor>::LAYOUT
    ) {}

    std::scoped_lock const lock{*_mutexPtr};

    auto& free = freeList<Descriptor>();
    auto const idxOpt = free.insert();
    JVK_STABLE_THROW(
        !idxOpt.has_value(),
        "every cell of the type is in use, got {} cells",
        free.size()
    ) {}

    try { _vk.write(*idxOpt, descriptor); }
    catch(...) {
        free.erase(*idxOpt);
        throw;
    }
    return ImageHandle<Descriptor>{*this, *idxOpt, descriptor};
}

template<cvk::descriptor_of<cvk::ResourceHeap> Descriptor>
void ResourceHeap::erase(uint32_t idx) const {
    std::scoped_lock const lock{*_mutexPtr};
    freeList<Descriptor>().erase(idx);
}

template<cvk::descriptor_of<cvk::ResourceHeap> Descriptor>
dev::free_list& ResourceHeap::freeList() const {
    if constexpr(std::same_as<Descriptor, cvk::SampledImageDescriptor>)
        return _sampledImages;
    else if constexpr(std::same_as<Descriptor, cvk::StorageImageDescriptor>)
        return _storageImages;
    else
        return _inputAttachments;
}

void ResourceHeap::forget() {
    auto const& config = _vk.config();
    _sampledImages = dev::free_list{config.sampledImages};
    _storageImages = dev::free_list{config.storageImages};
    _inputAttachments = dev::free_list{config.inputAttachments};
}

template void ResourceHeap::erase<cvk::SampledImageDescriptor>(uint32_t) const;
template void ResourceHeap::erase<cvk::StorageImageDescriptor>(uint32_t) const;
template void ResourceHeap::erase<cvk::InputAttachmentDescriptor>(uint32_t) const;
}
