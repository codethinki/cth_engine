#include "CthAttachmentCollection.hpp"

#include "vulkan/resource/image/CthImage.hpp"
#include "vulkan/resource/image/CthImageView.hpp"
#include "vulkan/utility/cth_vk_utils.hpp"


namespace cth::vk {

auto AttachmentDescription::create(VkFormat format, VkImageLayout initial_layout) const -> VkAttachmentDescription {
    return VkAttachmentDescription{
        .flags = flags,
        .format = format,
        .samples = samples,
        .loadOp = loadOp,
        .storeOp = storeOp,
        .stencilLoadOp = stencilLoadOp,
        .stencilStoreOp = stencilStoreOp,
        .initialLayout = initial_layout,
        .finalLayout = finalLayout
    };
}
}

namespace cth::vk {

AttachmentCollection::AttachmentCollection(not_null<BasicCore const*> core, size_t size, uint32_t render_pass_index,
    Image::Config const& image_config, AttachmentDescription const& description) : _core{core}, _config{image_config},
    _renderPassIndex{render_pass_index}, _size{size}, _description{description} {
    _images.reserve(size);
    _views.reserve(size);
}
AttachmentCollection::AttachmentCollection(not_null<BasicCore const*> core, size_t size, uint32_t render_pass_index,
    Image::Config const& image_config, AttachmentDescription const& description, VkExtent2D extent) :
    AttachmentCollection{core, size, render_pass_index, image_config, description} { create(extent); }

AttachmentCollection::AttachmentCollection(not_null<BasicCore const*> core, size_t size, uint32_t render_pass_index,
    Image::Config const& image_config, AttachmentDescription const& description, State const& state) :
    AttachmentCollection{core, size, render_pass_index, image_config, description} { wrap(state); }

AttachmentCollection::~AttachmentCollection() { optDestroy(); }


void AttachmentCollection::create(VkExtent2D extent) {
    optDestroy();
    _extent = extent;

    createImages();
    createImageViews();
}

void AttachmentCollection::wrap(State const& state) {
    optDestroy();

    auto const& imageHandles = state.vkImages;
    auto const& memoryHandles = state.vkMemoryHandles;
    auto const& viewHandles = state.vkImageViews;

    CTH_ERR(imageHandles.size() != _size, "size() image_handles required, image_handles: ({})", imageHandles.size()) throw details->exception();
    CTH_ERR(!memoryHandles.empty() && imageHandles.size() != memoryHandles.size(), "vkMemoryHandles.empty() || "
        "[vkMemoryHandles.size()] ({1}) ==  ({0}) [vkImages.size()] required", imageHandles.size(),
        memoryHandles.size())
        throw details->exception();
    CTH_ERR(viewHandles.size() != _size && !viewHandles.empty(), "0 or size() image view_handles required, image view_handles: ({})",
        viewHandles.size()) throw details->exception();

    Image::State imageState{
        .extent = state.extent,
        .vkImage = VK_NULL_HANDLE,
        .levelLayouts = std::vector{_config.mipLevels, _config.initialLayout},
        .bound = true,
        .memoryState = Memory::State{VK_NULL_HANDLE, state.byteSize},
    };


    for(size_t i = 0; i < imageHandles.size(); ++i) {
        imageState.vkImage = static_cast<gsl::owner<VkImage>>(imageHandles[i]);

        if(!memoryHandles.empty())
            imageState.memoryState.vkMemory = static_cast<gsl::owner<VkDeviceMemory>>(memoryHandles[i]);
        _images.emplace_back(_core, _config, imageState);
    }


    if(!viewHandles.empty())
        for(auto const [view, image] : std::views::zip(viewHandles, _images))
            _views.emplace_back(_core, ImageView::Config::Default(), ImageView::State{static_cast<gsl::owner<VkImageView>>(view), &image});
    else createImageViews();

}
void AttachmentCollection::destroy() {
    DEBUG_CHECK_ATTACHMENT_COLLECTION(this);

    _images.clear();
    _views.clear();
}
AttachmentCollection::State AttachmentCollection::release() {
    DEBUG_CHECK_ATTACHMENT_COLLECTION(this);

    State state{};
    state.extent = _extent;
    state.byteSize = _images[0].memory()->size();

    state.vkImages.resize(_images.size());
    state.vkMemoryHandles.resize(_images.size());
    state.vkImageViews.resize(_views.size());


    for(auto [image, handle, memory] : std::views::zip(_images, state.vkImages, state.vkMemoryHandles)) {
        // ReSharper disable once CppUseStructuredBinding
        Image::State const& imageState = image.release();
        handle = imageState.vkImage;
        memory = imageState.memoryState.vkMemory;
    }
    for(auto [view, handle] : std::views::zip(_views, state.vkImageViews)) {
        // ReSharper disable once CppUseStructuredBinding
        ImageView::State const& imageState = view.release();
        handle = imageState.vkImageView;
    }

    reset();

    return state;
}

void AttachmentCollection::reset() {
    _extent = {0, 0};
    _images.clear();
    _views.clear();
}
void AttachmentCollection::createImages() {
    size_t i = 0;
    for(; i < _size; ++i) _images.emplace_back(_core, _config, _extent);
}
void AttachmentCollection::createImageViews() {
    size_t i = 0;
    for(; i < _size; ++i) _views.emplace_back(_core, ImageView::Config::Default(), &_images[i]);
}

#ifdef CONSTANT_DEBUG_MODE
void AttachmentCollection::debug_check(AttachmentCollection const* collection) {
    CTH_ERR(collection == nullptr, "collection must not be invalid (nullptr)") throw details->exception();
    CTH_ERR(!collection->created(), "collection must have been created") throw details->exception();
}
#endif

}
