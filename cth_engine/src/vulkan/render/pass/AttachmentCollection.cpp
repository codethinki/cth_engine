#include "CthAttachmentCollection.hpp"

#include "vulkan/resource/image/CthImage.hpp"
#include "vulkan/resource/image/CthImageView.hpp"
#include "vulkan/utility/cth_vk_exceptions.hpp"


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

AttachmentCollection::AttachmentCollection(cth::not_null<Core const*> core, size_t size, uint32_t render_pass_index,
    Image::Config const& image_config, AttachmentDescription const& description) : _core{core}, _config{image_config},
    _renderPassIndex{render_pass_index}, _size{size}, _description{description}, _images{_size}, _views{_size} {}

AttachmentCollection::AttachmentCollection(cth::not_null<Core const*> core, size_t size, uint32_t render_pass_index,
    Image::Config const& image_config, AttachmentDescription const& description, VkExtent2D extent) :
    AttachmentCollection{core, size, render_pass_index, image_config, description} { create(extent); }

AttachmentCollection::AttachmentCollection(cth::not_null<Core const*> core, size_t size, uint32_t render_pass_index,
    Image::Config const& image_config, AttachmentDescription const& description, State state) :
    AttachmentCollection{core, size, render_pass_index, image_config, description} { wrap(std::move(state)); }

AttachmentCollection::~AttachmentCollection() { optDestroy(); }


void AttachmentCollection::create(VkExtent2D extent) {
    optDestroy();
    _extent = extent;

    createImages();
    createImageViews();
}

void AttachmentCollection::wrap(State state) {
    optDestroy();

    auto& images = state.images;
    auto& views = state.views;


    CTH_CRITICAL(images.size() != _size, "size() image_handles required, image_handles: ({})", images.size()) {}
    CTH_CRITICAL(views.size() != _size && !views.empty(), "0 or size() image view_handles required, image view_handles: ({})",
        views.size()) {}

    for(auto const& image : images) Image::debug_check(image.get());

    for(auto const [view, image] : std::views::zip(views, images)) {
        ImageView::debug_check(view.get());
        CTH_CRITICAL(view->image() != image.get(), "image and view mismatch") {}
    }

    for(auto [src, dst] : std::views::zip(images, _images))
        dst = src.release_val();

    if(views.empty()) createImageViews();
    else
        for(auto [src, dst] : std::views::zip(views, _views))
            dst = src.release_val();
}
void AttachmentCollection::destroy() {
    debug_check(this);

    std::ranges::fill(_images, nullptr);
    std::ranges::fill(_views, nullptr);
}
AttachmentCollection::State AttachmentCollection::release() {
    debug_check(this);

    State state{_extent};
    state.images.reserve(_size);
    state.views.reserve(_size);
    for(auto& image : _images) state.images.emplace_back(std::move(image));
    for(auto& view : _views) state.views.emplace_back(std::move(view));

    reset();

    return state;
}

void AttachmentCollection::reset() {
    _extent = {0, 0};
    std::ranges::fill(_images, nullptr);
    std::ranges::fill(_views, nullptr);
}
void AttachmentCollection::createImages() { for(size_t i = 0; i < _size; ++i) _images[i] = std::make_unique<Image>(_core, _config, _extent); }
void AttachmentCollection::createImageViews() {
    for(size_t i = 0; i < _size; ++i) {
        Image::debug_check(_images[i].get());
        _views[i] = std::make_unique<ImageView>(_core, ImageView::Config{}, _images[i].get());
    }
}

ImageView const* AttachmentCollection::view(size_t index) const {  return _views[index].get();  }
}
