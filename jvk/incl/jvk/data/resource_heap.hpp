#pragma once
#include "free_list.hpp"
#include "heap_idx.hpp"

#include "cvk/data/descriptor/descriptor.hpp"
#include "cvk/data/descriptor/image_descriptor.hpp"
#include "cvk/data/descriptor/resource_heap.hpp"
#include "cvk/data/img/image.hpp"

#include <volk.h>

#include <cth/io/log.hpp>
#include <cth/ptr/ref_wrapper.hpp>

#include <concepts>
#include <cstdint>
#include <memory>
#include <mutex>
#include <tuple>
#include <vector>


namespace cvk {
class Core;
}

namespace jvk {
/**
 * descriptor heap of the resources, places the descriptors
 * @details
 * - one free list per descriptor type, `insert()` returns the owning handle of a cell
 * - the destructor of a handle returns its cell, `Stage::retain()` keeps it while the gpu uses it
 * - the shader convention: `mappings()` and `jvk/heaps.glsl`
 * - owns a `cvk::ResourceHeap`, `vk()` reads it
 * - thread safe
 */
class ResourceHeap {
public:
    using Config = cvk::ResourceHeap::Config;

    template<cvk::descriptor_of<cvk::ResourceHeap> Descriptor>
    struct Cell;

    template<cvk::descriptor_of<cvk::ResourceHeap> Descriptor>
    class ImageHandle;

    using SampledImage = ImageHandle<cvk::SampledImageDescriptor>;
    using StorageImage = ImageHandle<cvk::StorageImageDescriptor>;
    using InputAttachment = ImageHandle<cvk::InputAttachmentDescriptor>;

    /**
     * creates the cvk heap
     * @pre `cvk::Core::created()`
     * @pre the preconditions of `cvk::ResourceHeap::create()`
     * @throws cvk::cvk_exception the exceptions of `cvk::ResourceHeap::create()`
     */
    ResourceHeap(cvk::Core const&, Config const&);

    /**
     * @pre no handle and no cell in a stasis of the heap is left
     */
    ~ResourceHeap();

    /**
     * places the descriptor into a free cell of its type
     * @return the owning handle of the cell
     * @pre the preconditions of `cvk::ResourceHeap::write()` besides the index
     * @throws jvk::jvk_exception reason: every cell of the type is in use
     * @throws cvk::vk_result_exception the exceptions of `cvk::ResourceHeap::write()`
     */
    [[nodiscard]] SampledImage insert(cvk::SampledImageDescriptor const&) const;

    /**
     * places the descriptor into a free cell of its type
     * @see @ref insert(cvk::SampledImageDescriptor const&) const
     */
    [[nodiscard]] StorageImage insert(cvk::StorageImageDescriptor const&) const;

    /**
     * places the descriptor into a free cell of its type
     * @see @ref insert(cvk::SampledImageDescriptor const&) const
     */
    [[nodiscard]] InputAttachment insert(cvk::InputAttachmentDescriptor const&) const;

    /**
     * @return the mappings of the shader convention: the sampled images and the storage images
     * @details set `constants::HEAP_SET`
     * @details bindings `constants::HEAP_SAMPLED_IMAGE_BINDING`, `constants::HEAP_STORAGE_IMAGE_BINDING`
     * @see `jvk/heaps.glsl`, `SamplerHeap::mappings()`
     */
    [[nodiscard]] std::vector<VkDescriptorSetAndBindingMappingEXT> mappings() const;

    /**
     * maps a `subpassInput` at the set and binding onto the input attachment at an index in the push data
     * @details push `InputAttachment::idx()` at `pushOffset`
     * @param set of the shader variable
     * @param binding of the shader variable
     * @param pushOffset of the index in the push data, in bytes
     * @pre the preconditions of `cvk::ResourceHeap::pushMapping()`
     */
    [[nodiscard]] VkDescriptorSetAndBindingMappingEXT pushMapping(
        uint32_t set,
        uint32_t binding,
        uint32_t pushOffset
    ) const;

    /**
     * the cvk heap, for binds and inheritances
     * @details const: no write of a cvk descriptor bypasses the free lists
     */
    [[nodiscard]] constexpr cvk::ResourceHeap const& vk() const { return _vk; }

    [[nodiscard]] constexpr Config const& config() const { return _vk.config(); }

private:
    template<cvk::descriptor_of<cvk::ResourceHeap> Descriptor>
    [[nodiscard]] ImageHandle<Descriptor> insertImage(Descriptor const&) const;

    /**
     * returns the cell to the free list of its type
     */
    template<cvk::descriptor_of<cvk::ResourceHeap> Descriptor>
    void erase(uint32_t idx) const;

    /**
     * @return the free list of the type
     * @pre `_mutex` is locked
     */
    template<cvk::descriptor_of<cvk::ResourceHeap> Descriptor>
    [[nodiscard]] dev::free_list& cells() const;

    mutable std::mutex _mutex{};
    mutable cvk::ResourceHeap _vk; ///< written under `_mutex`
    mutable dev::free_list _sampledImages; ///< under `_mutex`
    mutable dev::free_list _storageImages; ///< under `_mutex`
    mutable dev::free_list _inputAttachments; ///< under `_mutex`

public:
    ResourceHeap(ResourceHeap const& other) = delete;
    ResourceHeap(ResourceHeap&& other) = delete;
    ResourceHeap& operator=(ResourceHeap const& other) = delete;
    ResourceHeap& operator=(ResourceHeap&& other) = delete;
};
}

namespace jvk {
/**
 * cell of a descriptor type in the heap, for `Stasis`
 */
template<cvk::descriptor_of<cvk::ResourceHeap> Descriptor>
struct ResourceHeap::Cell {
    cth::ref_wrapper<ResourceHeap const> heap;
    uint32_t idx;

    /**
     * returns the cell to the free list of its type
     * @pre the gpu does not use the cell
     */
    void erase() const { heap->erase<Descriptor>(idx); }
};

/**
 * owning handle of a cell of the heap
 * @details
 * - the destructor returns the cell to the free list
 * - keeps the image, the subresource range of the view and the layout for the tracking, see `PrimaryCmdBuffer::use()`
 * - release it with `Stage::retain()` while the gpu uses it
 * - move only, a moved-from handle owns no cell
 * @tparam Descriptor type of the descriptor in the cell
 */
template<cvk::descriptor_of<cvk::ResourceHeap> Descriptor>
class ResourceHeap::ImageHandle {
public:
    struct State;

    /**
     * owns the cell with the descriptor
     * @details `ResourceHeap::insert()` constructs it
     * @pre the heap handed out the cell for this handle
     */
    ImageHandle(ResourceHeap const& heap, uint32_t idx, Descriptor const& descriptor) :
        _cell{&heap, eraser{idx}},
        _image{descriptor.image},
        _range{descriptor.view.range(descriptor.image->aspectMask())},
        _layout{descriptor.layout} {}

    ~ImageHandle() = default;

    /**
     * gives the cell to the state, does not erase it
     * @pre the handle owns a cell
     */
    [[nodiscard]] State release();

    /**
     * @return index of the cell, for the push data
     */
    [[nodiscard]] constexpr heap_idx<ImageHandle> idx() const {
        return heap_idx<ImageHandle>{_cell.get_deleter().idx};
    }

    [[nodiscard]] constexpr cvk::Image const& image() const { return *_image; }

    /**
     * @return the subresource range of the view in the image
     */
    [[nodiscard]] constexpr VkImageSubresourceRange const& range() const { return _range; }

    /**
     * @return layout of the image at the shader access, from the descriptor
     */
    [[nodiscard]] constexpr VkImageLayout layout() const { return _layout; }

private:
    /**
     * erases the cell of the handle
     */
    struct eraser {
        uint32_t idx;

        void operator()(ResourceHeap const* heap) const { heap->erase<Descriptor>(idx); }
    };

    std::unique_ptr<ResourceHeap const, eraser> _cell;
    cth::ref_wrapper<cvk::Image const> _image;
    VkImageSubresourceRange _range;
    VkImageLayout _layout;

public:
    ImageHandle(ImageHandle const& other) = delete;
    ImageHandle(ImageHandle&& other) noexcept = default;
    ImageHandle& operator=(ImageHandle const& other) = delete;
    ImageHandle& operator=(ImageHandle&& other) noexcept = default;
};

template<cvk::descriptor_of<cvk::ResourceHeap> Descriptor>
struct ResourceHeap::ImageHandle<Descriptor>::State {
    Cell<Descriptor> cell;

    /**
     * the cell to erase
     */
    [[nodiscard]] constexpr std::tuple<Cell<Descriptor>> decompose() && { return {cell}; }
};

template<cvk::descriptor_of<cvk::ResourceHeap> Descriptor>
auto ResourceHeap::ImageHandle<Descriptor>::release() -> State {
    CTH_CRITICAL(_cell == nullptr, "the handle must own a cell") {}
    auto const idx = _cell.get_deleter().idx;
    return State{.cell = {.heap = *_cell.release(), .idx = idx}};
}
}

namespace jvk {
using SampledImage = ResourceHeap::SampledImage;
using StorageImage = ResourceHeap::StorageImage;
using InputAttachment = ResourceHeap::InputAttachment;
}
