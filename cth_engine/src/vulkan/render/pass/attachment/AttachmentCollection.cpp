#include "AttachmentCollection.hpp"

#include "cth/numeric.hpp"

#include "AttachmentDescription.hpp"
#include "src/vulkan/resource/image/CthImage.hpp"
#include "src/vulkan/resource/image/CthImageView.hpp"
#include "src/vulkan/utility/cth_vk_exceptions.hpp"

namespace cth::vk {

AttachmentCollection::AttachmentCollection(Core const& core, Config config) : _core{&core}, _config{std::move(config)}, _images{total_size()} {
    Config::debug_check(_config);
    std::ranges::sort(_config.attachmentIndices);

    init();
}

AttachmentCollection::AttachmentCollection(Core const& core, Config const& config, VkExtent2D extent) :
    AttachmentCollection{core, config} { create(extent); }

AttachmentCollection::AttachmentCollection(Core const& core, Config const& config, State state) :
    AttachmentCollection{core, config} { wrap(std::move(state)); }

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


    CTH_CRITICAL(images.size() != total_size(), "size() * attachments() [{}] image_handles required, image_handles: ({})", total_size(),
        images.size()) {}
    CTH_CRITICAL(views.size() != total_size() && !views.empty(),
        "0 or size() * attachments() [{}] image view_handles required, image view_handles: ({})",
        total_size(), views.size()) {}

    for(auto const& image : images) Image::debug_check(*image);

    for(auto const [view, image] : std::views::zip(views, images)) {
        ImageView::debug_check(*view);
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
    state.images.reserve(total_size());
    state.views.reserve(total_size());
    for(auto& image : _images) state.images.emplace_back(std::move(image));
    for(auto& view : _views) state.views.emplace_back(std::make_unique<ImageView>(std::move(view)));

    reset();

    return state;
}
void AttachmentCollection::init() {
    _views.reserve(total_size());
    for(size_t i = 0; i < total_size(); i++)
        _views.emplace_back(*_core, ImageView::Config{});
}

void AttachmentCollection::reset() {
    _extent = {0, 0};
    std::ranges::fill(_images, nullptr);
}


void AttachmentCollection::createImages() { for(auto& image : _images) image = std::make_unique<Image>(*_core, _config.imageConfig, _extent); }
void AttachmentCollection::createImageViews() {
    for(auto [view, image] : std::views::zip(_views, _images)) {
        Image::debug_check(*image);
        view.create(*image);
    }
}
size_t AttachmentCollection::index_of(size_t sub_index, size_t index_nr) const {
    CTH_CRITICAL(!cth::num::in(sub_index, 0, _config.attachmentsPerIndex), "sub index [{}] out of bounds [0, {})", sub_index, _config.attachmentsPerIndex){}
    CTH_CRITICAL(!cth::num::in(index_nr, 0, attachments()), "attachment_nr [{}] out of bounds [0, {})", index_nr, 0, attachments());

    return index_nr * _config.attachmentsPerIndex + sub_index;
}

bool AttachmentCollection::created() const { return _views[0].created(); }

ImageView const& AttachmentCollection::view(size_t sub_index, size_t att_index) const { return _views[index_of(sub_index, att_index)]; }

Image* AttachmentCollection::image(size_t sub_index, size_t att_index) const { return _images[index_of(sub_index, att_index)].get(); }


std::vector<VkAttachmentReference> AttachmentCollection::references() const {
    return {
        std::from_range,
        _config.attachmentIndices | std::views::transform([this](auto index) {
            return VkAttachmentReference{.attachment = index, .layout = _config.description.referenceLayout};
        })
    };
}
}
