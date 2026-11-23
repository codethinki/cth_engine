#pragma once
#include "free_list.hpp"
#include "heap_idx.hpp"

#include "cvk/data/descriptor/descriptor.hpp"
#include "cvk/data/descriptor/image_descriptor.hpp"
#include "cvk/data/descriptor/resource_heap.hpp"
#include "cvk/data/img/image.hpp"
#include "cvk/utility/constants.hpp"

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
 * - `insert()` and the handles: thread safe
 */
class ResourceHeap {
public:
    using Config = cvk::ResourceHeap::Config;

    template<cvk::descriptor_of<cvk::ResourceHeap> Descriptor>
    struct Cell;

    /**
     * deleter of a `unique_cell`, returns the cell to the free list of its type
     * @details nested in the heap: it reaches the private erase of the heap
     */
    template<cvk::descriptor_of<cvk::ResourceHeap> Descriptor>
    struct cell_eraser {
        uint32_t idx; ///< of the cell

        void operator()(ResourceHeap const* heap) const;
    };

    /**
     * owner of a cell: the deleter returns the cell to the free list
     * @details the pointer is the heap, the deleter holds the index of the cell
     * @details the heap and the image stay, nothing but the cell goes
     */
    template<cvk::descriptor_of<cvk::ResourceHeap> Descriptor>
    using unique_cell = std::unique_ptr<ResourceHeap const, cell_eraser<Descriptor>>;

    template<cvk::descriptor_of<cvk::ResourceHeap> Descriptor>
    class ImageHandle;

    using SampledImage = ImageHandle<cvk::SampledImageDescriptor>;
    using StorageImage = ImageHandle<cvk::StorageImageDescriptor>;
    using InputAttachment = ImageHandle<cvk::InputAttachmentDescriptor>;

    /**
     * base constructor
     */
    ResourceHeap(cvk::Core const&, Config const&);

    /**
     * constructs and creates
     * @details calls @ref ResourceHeap(cvk::Core const&, Config const&), `create()`
     */
    explicit ResourceHeap(cvk::Core const&, Config const&, cvk::create_t);

    /**
     * @note calls `optDestroy()`
     */
    ~ResourceHeap() { optDestroy(); }

    /**
     * creates the cvk heap, every cell is free
     * @pre `cvk::Core::created()`
     * @pre the preconditions of `cvk::ResourceHeap::create()`
     * @post `created()`
     * @note calls `optDestroy()`
     * @throws cvk::cvk_exception the exceptions of `cvk::ResourceHeap::create()`
     */
    void create();

    /**
     * destroys the cvk heap and forgets the cells
     * @pre `created()`
     * @pre no handle and no cell in a stasis of the heap is left
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
     * places the descriptor into a free cell of its type
     * @return the owning handle of the cell
     * @pre the layout of the descriptor is the `ImageHandle::LAYOUT` of its handle
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
     * @pre `created()`
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

    [[nodiscard]] constexpr bool created() const { return _vk.created(); }
    [[nodiscard]] constexpr Config const& config() const { return _vk.config(); }

private:
    void forget();

    template<cvk::descriptor_of<cvk::ResourceHeap> Descriptor>
    [[nodiscard]] ImageHandle<Descriptor> insertImage(Descriptor const&) const;

    template<cvk::descriptor_of<cvk::ResourceHeap> Descriptor>
    void erase(uint32_t idx) const;

    // the caller holds `_mutexPtr`
    template<cvk::descriptor_of<cvk::ResourceHeap> Descriptor>
    [[nodiscard]] dev::free_list& freeList() const;

    // behind a std::unique_ptr: the mutex is not movable, the moves stay = default
    std::unique_ptr<std::mutex> _mutexPtr = std::make_unique<std::mutex>();
    mutable cvk::ResourceHeap _vk; // written under `_mutexPtr`
    mutable dev::free_list _sampledImages; // under `_mutexPtr`
    mutable dev::free_list _storageImages; // under `_mutexPtr`
    mutable dev::free_list _inputAttachments; // under `_mutexPtr`

public:
    ResourceHeap(ResourceHeap const& other) = delete;
    ResourceHeap(ResourceHeap&& other) noexcept = default;
    ResourceHeap& operator=(ResourceHeap const& other) = delete;
    ResourceHeap& operator=(ResourceHeap&& other) noexcept = default;
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

template<cvk::descriptor_of<cvk::ResourceHeap> Descriptor>
void ResourceHeap::cell_eraser<Descriptor>::operator()(ResourceHeap const* heap) const {
    heap->erase<Descriptor>(idx);
}

/**
 * owning handle of a cell of the heap
 * @details
 * - the destructor returns the cell to the free list
 * - keeps the image and the subresource range of the view for the tracking, see `PrimaryCmdBuffer::use()`
 * - the layout is fixed per type: `LAYOUT`
 * - release it with `Stage::retain()` while the gpu uses it
 * - move only, a moved-from handle owns no cell
 * @tparam Descriptor type of the descriptor in the cell
 */
template<cvk::descriptor_of<cvk::ResourceHeap> Descriptor>
class ResourceHeap::ImageHandle {
public:
    struct State;

    /**
     * layout of the image at the shader access, the descriptor has it
     * @details sampled image: `VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL`, also for a read-only depth attachment
     * @details storage image: `VK_IMAGE_LAYOUT_GENERAL`
     * @details input attachment: `VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR`
     */
    static constexpr VkImageLayout LAYOUT = [] {
        if constexpr(std::same_as<Descriptor, cvk::SampledImageDescriptor>)
            return VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
        else if constexpr(std::same_as<Descriptor, cvk::StorageImageDescriptor>)
            return VK_IMAGE_LAYOUT_GENERAL;
        else
            return VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR;
    }();

    /**
     * owns the cell with the descriptor
     * @details `ResourceHeap::insert()` constructs it
     * @pre the heap handed out the cell for this handle
     */
    ImageHandle(ResourceHeap const& heap, uint32_t idx, Descriptor const& descriptor) :
        _cell{&heap, cell_eraser<Descriptor>{idx}},
        _image{descriptor.image},
        _range{descriptor.view.range(descriptor.image->aspectMask())} {}

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

private:
    unique_cell<Descriptor> _cell;
    cth::ref_wrapper<cvk::Image const> _image;
    VkImageSubresourceRange _range;

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
