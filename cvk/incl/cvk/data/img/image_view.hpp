#pragma once
#include "image_view_config.hpp"

#include "cvk/base/device_table.hpp"
#include "cvk/data/img/image.hpp"
#include "cvk/utility/constants.hpp"
#include "cvk/utility/handle/core_handle.hpp"
#include "cvk/utility/handle/destroy.hpp"
#include "cvk/utility/types.hpp"

#include <volk.h>

#include <cth/io/log.hpp>
#include <cth/ptr/ref_wrapper.hpp>

#include <tuple>


namespace cvk {
/**
 * `VkImageView` wrapper
 */
class ImageView {
public:
    using Config = ImageViewConfig;
    struct State;

    /**
     * base constructor
     */
    ImageView(Image const&, Config const&);

    /**
     * constructs and wraps
     * @details calls @ref ImageView(Image const&, Config const&), `wrap()`
     */
    ImageView(Image const&, Config const&, State const&);

    /**
     * constructs and creates
     * @details calls @ref ImageView(Image const&, Config const&), `create()`
     */
    explicit ImageView(Image const&, Config const&, create_t);

    /**
     * @note calls `optDestroy()`
     */
    ~ImageView() { optDestroy(); }

    /**
     * wraps the state
     * @pre the view was created for the image with the config
     * @post `created()`
     * @note calls `optDestroy()`
     */
    void wrap(State const&);

    /**
     * creates the view
     * @pre `Image::created()`
     * @pre the mip range and the layer range lie in the image
     * @pre the layers fit the view type
     * @post `created()`
     * @note calls `optDestroy()`
     * @throws cvk::vk_result_exception result of `vkCreateImageView()`
     */
    void create();

    /**
     * destroys and forgets
     * @pre `created()`
     * @pre the gpu does not use the view
     */
    void destroy();

    /**
     * if `created()` -> calls `destroy()`
     */
    void optDestroy() { if(created()) destroy(); }

    /**
     * releases ownership, returns the state and forgets
     * @pre `created()`
     */
    State release();

    [[nodiscard]] constexpr bool created() const { return static_cast<bool>(_handle); }
    [[nodiscard]] constexpr VkImageView get() const { return _handle.get(); }
    [[nodiscard]] constexpr Core const& core() const { return _handle.core(); }
    [[nodiscard]] constexpr Image const& image() const { return *_image; }
    [[nodiscard]] constexpr Config const& config() const { return _config; }

    /**
     * @return the subresource range of the view in the image
     */
    [[nodiscard]] constexpr VkImageSubresourceRange range() const { return _config.range(_image->aspectMask()); }

    /**
     * @return the layers of the view
     */
    [[nodiscard]] constexpr uint32_t arrayLayers() const { return _config.layerCount(*_image); }

    /**
     * @return `Config::viewType`, `VK_IMAGE_VIEW_TYPE_MAX_ENUM` -> inferred
     * @see `ImageViewConfig::resolvedViewType()`
     */
    [[nodiscard]] constexpr VkImageViewType viewType() const { return _config.resolvedViewType(*_image); }

private:
    void forget();

    core_handle<VkImageView> _handle;
    cth::ref_wrapper<Image const> _image;
    Config _config;

public:
    ImageView(ImageView const& other) = delete;
    ImageView(ImageView&& other) noexcept = default;
    ImageView& operator=(ImageView const& other) = delete;
    ImageView& operator=(ImageView&& other) noexcept = default;

    static constexpr void debug_check(ImageView const&);
};
}

namespace cvk {
struct ImageView::State {
    vk_not_null<VkImageView> vkImageView;

    /**
     * the raw handles to destroy, in destruction order
     */
    [[nodiscard]] constexpr std::tuple<VkImageView> decompose() && { return {vkImageView.get()}; }
};
}

namespace cvk {
constexpr void ImageView::debug_check(ImageView const& view) {
    CTH_CRITICAL(!view.created(), "image view must be created") {}
}
}
