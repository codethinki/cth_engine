#include "CthImageView.hpp"

#include "CthImage.hpp"
#include "vulkan/base/CthCore.hpp"
#include "vulkan/utility/cth_vk_utils.hpp"



namespace cth::vk {
ImageView::ImageView(BasicCore const* device, BasicImage const* image, Config const& config) : _core(device), _image(image) {
    CTH_ERR(image == nullptr, "image ptr not valid") throw details->exception();

    create(config);
}

ImageView::~ImageView() {
    if(_handle != nullptr) vkDestroyImageView(_core->vkDevice(), _handle.get(), nullptr);
}

void ImageView::create(Config const& config) {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = _image->get();
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = _image->format();

    viewInfo.subresourceRange = config.range();

    auto const levels = config.levelCount == 0 ? _image->mipLevels() - config.baseMipLevel : config.levelCount;
    viewInfo.subresourceRange.levelCount = levels;

    auto const mask = _image->aspectMask(); //config.range.aspectMask == VK_IMAGE_ASPECT_NONE ? _image->aspectMask() : config.range.aspectMask;
    viewInfo.subresourceRange.aspectMask = mask;

    auto const layers = 1; //config.range.layerCount == 0 ? config.range.baseArrayLayer - _image->arrayLayers() : config.range.layerCount;
    viewInfo.subresourceRange.layerCount = layers;

    VkImageView handle = VK_NULL_HANDLE;
    auto const result = vkCreateImageView(_core->vkDevice(), &viewInfo, nullptr, &handle);

    CTH_STABLE_ERR(result != VK_SUCCESS, "failed to create image-view")
        throw except::vk_result_exception{result, details->exception()};

    _handle = handle;

}
} // namespace cth
