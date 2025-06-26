#include "AttachmentCollection.hpp"

#include "src/vulkan/resource/image/CthImage.hpp"
#include "src/vulkan/resource/image/CthImageView.hpp"
#include "src/vulkan/utility/cth_vk_exceptions.hpp"


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

AttachmentCollection::AttachmentCollection(Core const& core, size_t size, uint32_t render_pass_index,
    Image::Config const& image_config, AttachmentDescription const& description) : _core{&core}, _config{image_config},
    _renderPassIndex{render_pass_index}, _size{size}, _description{description}, _images{_size} { init(); }

AttachmentCollection::AttachmentCollection(Core const& core, size_t size, uint32_t render_pass_index,
    Image::Config const& image_config, AttachmentDescription const& description, VkExtent2D extent) :
    AttachmentCollection{core, size, render_pass_index, image_config, description} { create(extent); }

AttachmentCollection::AttachmentCollection(Core const& core, size_t size, uint32_t render_pass_index,
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

    for(auto const& image : images) Image::debug_check(*image);

    for(auto const [view, image] : std::views::zip(views, images)) {
        ImageView::debug_check(view.get());
        CTH_CRITICAL(view->image() != image.get(), "image and view mismatch") {}
    }

    for(auto [src, dst] : std::views::zip(images, _images))
        dst = src.release_val();

    if(views.empty()) createImageViews();
    else
        for(auto [src, dst] : std::views::zip(views, _views))
            dst = std::move(*src.release_val());
}
void AttachmentCollection::destroy() {
    debug_check(*this);

    std::ranges::fill(_images, nullptr);
    for(auto& view : _views) view.destroy();

    reset();
}
AttachmentCollection::State AttachmentCollection::release() {
    debug_check(*this);

    State state{_extent};
    state.images.reserve(_size);
    state.views.reserve(_size);
    for(auto& image : _images) state.images.emplace_back(std::move(image));
    for(auto& view : _views) state.views.emplace_back(std::make_unique<ImageView>(std::move(view)));

    reset();

    return state;
}
void AttachmentCollection::init() {
    _views.reserve(_size);
    for(size_t i = 0; i < _size; i++)
        _views.emplace_back(*_core, ImageView::Config{});
}

void AttachmentCollection::reset() {
    _extent = {0, 0};
    std::ranges::fill(_images, nullptr);
}


void AttachmentCollection::createImages() { for(size_t i = 0; i < _size; ++i) _images[i] = std::make_unique<Image>(*_core, _config, _extent); }
void AttachmentCollection::createImageViews() {
    for(size_t i = 0; i < _size; ++i) {
        auto& image = *_images[i];
        Image::debug_check(image);
        _views[i].create(image);
    }
}

bool AttachmentCollection::created() const {
    return _views[0].created();
}
ImageView const& AttachmentCollection::view(size_t index) const { return _views[index]; }
}
