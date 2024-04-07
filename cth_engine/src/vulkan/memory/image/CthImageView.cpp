#include "CthImageView.hpp"


#include "CthImage.hpp"
#include "vulkan/base/CthDevice.hpp"
#include "vulkan/utility/CthVkUtils.hpp"

#include <cth/cth_log.hpp>


namespace cth {
ImageView::ImageView(Image* image, const Config& config) : _image(image) {
    CTH_ERR(image != nullptr, "image ptr not valid") throw details->exception();

    create(config);
}
ImageView::~ImageView() { vkDestroyImageView(_image->device->get(), vkImageView, nullptr); }

void ImageView::create(const Config& config) {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = _image->get();
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = _image->format();

    viewInfo.subresourceRange = config.range();

    const auto levels = config.levelCount == 0 ? config.baseMipLevel - _image->mipLevels() : config.levelCount;
    viewInfo.subresourceRange.levelCount = levels;

    const auto mask = _image->aspectMask(); //config.range.aspectMask == VK_IMAGE_ASPECT_NONE ? _image->aspectMask() : config.range.aspectMask;
    viewInfo.subresourceRange.aspectMask = mask;

    const auto layers = 1; //config.range.layerCount == 0 ? config.range.baseArrayLayer - _image->arrayLayers() : config.range.layerCount;
    viewInfo.subresourceRange.layerCount = layers;

    const auto createResult = vkCreateImageView(_image->device->get(), &viewInfo, nullptr, &vkImageView);
    CTH_STABLE_ERR(createResult != VK_SUCCESS, "failed to create image-view")
        throw except::vk_result_exception{createResult, details->exception()};

}
} // namespace cth
