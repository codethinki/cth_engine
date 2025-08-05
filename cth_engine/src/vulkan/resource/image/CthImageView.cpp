#include "CthImageView.hpp"

#include "CthImage.hpp"

#include "../CthDestructionQueue.hpp"

#include "src/vulkan/base/CthCore.hpp"
#include "src/vulkan/base/CthDevice.hpp"
#include "src/vulkan/utility/cth_vk_exceptions.hpp"



namespace cth::vk {
ImageView::ImageView(Core const& core, Config const& config) : _core{&core}, _config{config} { Core::debug_check(core); }
ImageView::ImageView(Core const& core, Config const& config, Image const& image) : ImageView{core, config} { create(image); }
ImageView::ImageView(Core const& core, Config const& config, State const& state) : ImageView{core, config} { wrap(state); }

ImageView::~ImageView() { optDestroy(); }

void ImageView::create(Image const& image) {
    Image::debug_check(image);
    optDestroy();

    _image = &image;

    auto const viewInfo = createViewInfo();

    VkImageView handle = VK_NULL_HANDLE;
    auto const result = _core->functions()->vkCreateImageView(_core->vkDevice(), &viewInfo, nullptr, &handle);

    CTH_STABLE_ERR(result != VK_SUCCESS, "failed to create vk_image-view") {
        reset();
        throw vk::result_exception{result, details->exception()};
    }

    _handle = handle;
}
void ImageView::wrap(State const& state) {
    optDestroy();

    _handle = state.vkImageView.get();
    _image = state.image.get();

}
void ImageView::destroy() {
    ImageView::debug_check(*this);
    auto const lambda = [table = _core->deviceTable(), vk_image_view = _handle.get()] { destroy(table, vk_image_view); };

    auto const queue = _core->destructionQueue();

    if(queue) queue->push(lambda);
    else lambda();

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

void ImageView::destroy(DeviceTable table, VkImageView vk_image_view) {
    CTH_WARN(vk_image_view == VK_NULL_HANDLE, "image view should not be invalid (VK_NULL_HANDLE)") {}

    table->vkDestroyImageView(table.device(), vk_image_view, nullptr);
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
