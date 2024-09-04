#include "CthImageView.hpp"

#include "CthImage.hpp"

#include "../CthDestructionQueue.hpp"

#include "vulkan/base/CthCore.hpp"
#include "vulkan/utility/cth_vk_utils.hpp"



namespace cth::vk {
ImageView::ImageView(cth::not_null<BasicCore const*> core, Config const& config) : _core(core), _config{config} {
    DEBUG_CHECK_CORE(core);
}
ImageView::ImageView(cth::not_null<BasicCore const*> core, Config const& config, Image const* image) : ImageView{core, config} { create(image); }
ImageView::ImageView(cth::not_null<BasicCore const*> core, Config const& config, State const& state) : ImageView{core, config} { wrap(state); }

ImageView::~ImageView() {
    optDestroy();
}

void ImageView::create(cth::not_null<Image const*> image) {
    DEBUG_CHECK_IMAGE(image);
   optDestroy();

    _image = image.get();

    auto const viewInfo = createViewInfo();

    VkImageView handle = VK_NULL_HANDLE;
    auto const result = vkCreateImageView(_core->vkDevice(), &viewInfo, nullptr, &handle);

    CTH_STABLE_ERR(result != VK_SUCCESS, "failed to create vk_image-view") {
        reset();
        throw vk::result_exception{result, details->exception()};
    }

    _handle = handle;
}
void ImageView::wrap(State const& state) {
    optDestroy();

    _handle = state.vkImageView;
    _image = state.image;

}
void ImageView::destroy() {
    DEBUG_CHECK_IMAGE_VIEW(this);

    auto const queue = _core->destructionQueue();

    if(queue != nullptr) queue->push(_handle.get());
    else destroy(_core->vkDevice(), _handle.get());

    reset();
}
ImageView::State ImageView::release() {
    State const state{
        _handle.get(),
        _image,
    };
    reset();
    return state;
}

void ImageView::destroy(VkDevice vk_device, VkImageView vk_image_view) {
    DEBUG_CHECK_DEVICE_HANDLE(vk_device);
    CTH_WARN(vk_image_view == VK_NULL_HANDLE, "image view should not be invalid (VK_NULL_HANDLE)") {}

    vkDestroyImageView(vk_device, vk_image_view, nullptr);
}

VkImageViewCreateInfo ImageView::createViewInfo() const {
    auto const levels = _config.levelCount == 0 ? _image->mipLevels() - _config.baseMipLevel : _config.levelCount;

    auto const mask = _image->aspectMask(); //config.range.aspectMask == VK_IMAGE_ASPECT_NONE ? _image->aspectMask() : config.range.aspectMask,

    [[maybe_unused]] auto constexpr layers = 1;
    //config.range.layerCount == 0 ? config.range.baseArrayLayer - _image->arrayLayers() : config.range.layerCount,


    return VkImageViewCreateInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = _image->get(),
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = _image->format(),

        .subresourceRange = _config.range(levels, mask),
    };
}
void ImageView::reset() {
    _handle = VK_NULL_HANDLE;
    _image = nullptr;
}

#ifdef CONSTANT_DEBUG_MODE
void ImageView::debug_check(ImageView const* image_view) {
    CTH_ERR(image_view == nullptr, "image view must not be invalid (nullptr)") throw details->exception();
    CTH_ERR(!image_view->created(), "image view must be created") throw details->exception();

    DEBUG_CHECK_IMAGE_VIEW_HANDLE(image_view->get());
}
void ImageView::debug_check_handle(VkImageView vk_image_view) {
    CTH_ERR(vk_image_view == VK_NULL_HANDLE, "image view handle must not be invalid (VK_NULL_HANDLE)")
        throw details->exception();
}
#endif
} // namespace cth


//Config

namespace cth::vk {
VkImageSubresourceRange ImageView::Config::range(uint32_t image_mip_levels, VkImageAspectFlags aspect_mask) const {
    return VkImageSubresourceRange{
        .aspectMask = aspect_mask,
        .baseMipLevel = baseMipLevel,
        .levelCount = image_mip_levels,
        .baseArrayLayer = 0,
        .layerCount = 1,
    };
}
ImageView::Config ImageView::Config::Default() {
    return Config{
        .baseMipLevel = 0,
        .levelCount = 0,
    };
}
}
