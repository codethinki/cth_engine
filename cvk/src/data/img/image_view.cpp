#include "cvk/data/img/image_view.hpp"

#include "cvk/base/core.hpp"
#include "cvk/utility/vk_exceptions.hpp"


namespace cvk {
ImageView::ImageView(Image const& image, Config const& config) : _handle{image.core()},
    _image{image},
    _config{config} {}

ImageView::ImageView(
    Image const& image,
    Config const& config,
    State const& state
) : ImageView{image, config} { wrap(state); }

ImageView::ImageView(
    Image const& image,
    Config const& config,
    create_t
) : ImageView{image, config} { create(); }

void ImageView::wrap(State const& state) {
    optDestroy();

    _handle.reset(state.vkImageView.get(), core());
}

void ImageView::create() {
    Config::debug_check(_config, *_image);

    optDestroy();

    auto const info = _config.createInfo(*_image);

    VkImageView ptr = VK_NULL_HANDLE;
    auto const result = core().functions()->vkCreateImageView(
        core().vkDevice(),
        &info,
        nullptr,
        &ptr
    );

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

void destroy(DeviceTable table, VkImageView vkImageView) {
    CTH_WARN(vkImageView == VK_NULL_HANDLE, "vkImageView invalid (VK_NULL_HANDLE)") {}

    table->vkDestroyImageView(table.device(), vkImageView, nullptr);
}

void ImageView::forget() {}
}
