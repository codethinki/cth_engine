#pragma once
#include "image_view_config.hpp"

#include "cvk/base/device_table.hpp"
#include "cvk/data/img/image.hpp"
#include "cvk/utility/constants.hpp"
#include "cvk/utility/handle/core_handle.hpp"
#include "cvk/utility/types.hpp"

#include <volk.h>

#include <cth/io/log.hpp>


namespace cvk {
/**
 * @brief VkImageView wrapper, view of a mip and layer range of its image
 */
class ImageView {
public:
    using vk_handle_t = VkImageView;
    using Config = ImageViewConfig;
    struct State;

    /**
     * @brief base constructor
     * @details the core comes from the image
     */
    ImageView(Image const&, Config const&);

    /**
     * @brief constructs and wraps
     * @details calls:
     * - @ref ImageView(Image const&, Config const&)
     * - @ref wrap()
     */
    ImageView(Image const&, Config const&, State const&);

    /**
     * @brief constructs and creates
     * @details calls:
     * - @ref ImageView(Image const&, Config const&)
     * - @ref create()
     */
    explicit ImageView(Image const&, Config const&, create_t);

    /**
     * @note calls @ref optDestroy()
     */
    ~ImageView() { optDestroy(); }

    /**
     * @brief wraps the state
     * @pre the view was created for the image with the config
     * @post @ref created()
     * @note calls @ref optDestroy()
     */
    void wrap(State const&);

    /**
     * @brief creates the view
     * @pre @ref Image::created()
     * @pre the mip range and the layer range lie in the image
     * @pre the layers fit the view type
     * @post @ref created()
     * @note calls @ref optDestroy()
     * @throws cvk::vk_result_exception result of vkCreateImageView()
     */
    void create();

    /**
     * @brief destroys and forgets
     * @pre @ref created()
     * @pre the gpu does not use the view
     */
    void destroy();

    /**
     * @brief if @ref created() -> calls @ref destroy()
     */
    void optDestroy() { if(created()) destroy(); }

    /**
     * @brief releases ownership, returns the state and forgets
     * @pre @ref created()
     */
    State release();

    static void destroy(DeviceTable table, VkImageView);

    [[nodiscard]] constexpr bool created() const { return static_cast<bool>(_handle); }
    [[nodiscard]] constexpr vk_handle_t get() const { return _handle.get(); }
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
    [[nodiscard]] constexpr uint32_t arrayLayers() const {
        return _config.arrayLayers == VK_REMAINING_ARRAY_LAYERS ? _image->arrayLayers() - _config.baseArrayLayer
            : _config.arrayLayers;
    }

    /**
     * @return @ref Config::viewType, VK_IMAGE_VIEW_TYPE_MAX_ENUM -> inferred:
     *  one layer -> 2D, cube compatible image with 6 layers -> cube, a multiple of 6 -> cube array, else 2D array
     */
    [[nodiscard]] constexpr VkImageViewType viewType() const {
        if(_config.viewType != VK_IMAGE_VIEW_TYPE_MAX_ENUM) return _config.viewType;

        auto const layers = arrayLayers();
        if(layers == 1) return VK_IMAGE_VIEW_TYPE_2D;
        if((_image->config().flags & VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT) == 0 || layers % 6 != 0)
            return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        return layers == 6 ? VK_IMAGE_VIEW_TYPE_CUBE : VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
    }

private:
    void forget();

    core_handle<ImageView> _handle;
    not_null_ptr<Image const> _image;
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
};
}

namespace cvk {
constexpr void ImageView::debug_check(ImageView const& view) {
    CTH_CRITICAL(!view.created(), "image view must be created") {}
}
}
