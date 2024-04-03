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
ImageView::~ImageView() {
    vkDestroyImageView(_image->device->get(), vkImageView, nullptr);
}

void ImageView::create(const Config& config) {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = _image->get();
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = _image->format();
    if(config.range.aspectMask & VK_IMAGE_ASPECT_NONE) viewInfo.subresourceRange.aspectMask = _image->aspectFlags();
    else viewInfo.subresourceRange.aspectMask = config.range.aspectMask;
    if(config.range.levelCount == 0) viewInfo.subresourceRange.levelCount = _image->mipLevels();
    else viewInfo.subresourceRange.levelCount = config.range.levelCount;
    if(config.range.layerCount == 0) viewInfo.subresourceRange.layerCount = 1;
    else viewInfo.subresourceRange.layerCount = config.range.layerCount;

    const auto createResult = vkCreateImageView(_image->device->get(), &viewInfo, nullptr, &vkImageView);
    CTH_STABLE_ERR(createResult != VK_SUCCESS, "failed to create image-view")
        throw except::vk_result_exception{createResult, details->exception()};

}
} // namespace cth
