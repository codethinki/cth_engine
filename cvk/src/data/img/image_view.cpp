#include "cvk/data/img/image_view.hpp"

#include "cvk/base/core.hpp"
#include "cvk/utility/vk_exceptions.hpp"


namespace cvk {
ImageView::ImageView(Image const& image, Config const& config) : _handle{image.core()}, _image{&image},
    _config{config} {}

ImageView::ImageView(Image const& image, Config const& config, State const& state) : ImageView{image, config} {
    wrap(state);
}

ImageView::ImageView(Image const& image, Config const& config, create_t) : ImageView{image, config} { create(); }

void ImageView::wrap(State const& state) {
    optDestroy();

    _handle.reset(state.vkImageView.get(), core());
}

void ImageView::create() {
    Image::debug_check(*_image);
    CTH_CRITICAL(_config.baseMipLevel >= _image->mipLevels(), "base mip level must lie in the image") {}
    CTH_CRITICAL(
        _config.mipLevels != VK_REMAINING_MIP_LEVELS && _config.mipLevels > _image->mipLevels() - _config.baseMipLevel,
        "mip levels must lie in the image"
    ) {}
    CTH_CRITICAL(_config.baseArrayLayer >= _image->arrayLayers(), "base array layer must lie in the image") {}
    CTH_CRITICAL(
        _config.arrayLayers != VK_REMAINING_ARRAY_LAYERS
        && _config.arrayLayers > _image->arrayLayers() - _config.baseArrayLayer,
        "array layers must lie in the image"
    ) {}

    optDestroy();

    VkImageViewCreateInfo const info{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .image = _image->get(),
        .viewType = viewType(),
        .format = _image->format(),
        .components = {
            .r = VK_COMPONENT_SWIZZLE_IDENTITY,
            .g = VK_COMPONENT_SWIZZLE_IDENTITY,
            .b = VK_COMPONENT_SWIZZLE_IDENTITY,
            .a = VK_COMPONENT_SWIZZLE_IDENTITY,
        },
        .subresourceRange = range(),
    };

    VkImageView ptr = VK_NULL_HANDLE;
    auto const result = core().functions()->vkCreateImageView(core().vkDevice(), &info, nullptr, &ptr);

    CTH_STABLE_ERR(result != VK_SUCCESS, "failed to create image view") {
        forget();
        throw cvk::vk_result_exception{result, details->exception()};
    }

    _handle.reset(ptr, core());
}

void ImageView::destroy() {
    debug_check(*this);

    _handle.reset();

    forget();
}

ImageView::State ImageView::release() {
    debug_check(*this);

    State const state{_handle.release()};
    forget();
    return state;
}

void ImageView::destroy(DeviceTable table, VkImageView vkImageView) {
    CTH_WARN(vkImageView == VK_NULL_HANDLE, "vkImageView invalid (VK_NULL_HANDLE)") {}

    table->vkDestroyImageView(table.device(), vkImageView, nullptr);
}

void ImageView::forget() {}
}
