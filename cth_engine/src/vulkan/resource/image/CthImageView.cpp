#include "CthImageView.hpp"

#include "CthImage.hpp"
#include "vulkan/base/CthCore.hpp"
#include "vulkan/utility/CthVkUtils.hpp"



namespace cth {
ImageView::ImageView(const BasicCore* device, const BasicImage* image, const Config& config) : _core(device), _image(image) {
    CTH_ERR(image == nullptr, "image ptr not valid") throw details->exception();

    create(config);
}

ImageView::~ImageView() {
    if(_handle != nullptr) vkDestroyImageView(_core->vkDevice(), _handle.get(), nullptr);
}

void ImageView::create(const Config& config) {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = _image->get();
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = _image->format();

    viewInfo.subresourceRange = config.range();

    const auto levels = config.levelCount == 0 ? _image->mipLevels() - config.baseMipLevel : config.levelCount;
    viewInfo.subresourceRange.levelCount = levels;

    const auto mask = _image->aspectMask(); //config.range.aspectMask == VK_IMAGE_ASPECT_NONE ? _image->aspectMask() : config.range.aspectMask;
    viewInfo.subresourceRange.aspectMask = mask;

    const auto layers = 1; //config.range.layerCount == 0 ? config.range.baseArrayLayer - _image->arrayLayers() : config.range.layerCount;
    viewInfo.subresourceRange.layerCount = layers;

    VkImageView handle = VK_NULL_HANDLE;
    const auto result = vkCreateImageView(_core->vkDevice(), &viewInfo, nullptr, &handle);

    CTH_STABLE_ERR(result != VK_SUCCESS, "failed to create image-view")
        throw except::vk_result_exception{result, details->exception()};

    _handle = handle;

}
} // namespace cth
