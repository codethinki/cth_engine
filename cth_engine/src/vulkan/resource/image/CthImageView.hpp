#pragma once
#include "vulkan/utility/cth_constants.hpp"
#include "vulkan/utility/cth_vk_types.hpp"

#include <vulkan/vulkan.h>

namespace cth::vk {
class Core;
class Image;
class Device;

class ImageView {
public:
    struct Config;
    struct State;

    /**
     * @brief base constructor
     * @param core must be created
     */
    ImageView(cth::not_null<Core const*> core, Config const& config);

    /**
     * @brief constructs and creates
     * @note calls @ref create()
     * @note calls @ref ImageView(cth::not_null<Core const*>, Config const&)
     */
    ImageView(cth::not_null<Core const*> core, Config const& config, cth::not_null<Image const*> image);

    /**
     * @brief constructs and wraps state
     * @note calls @ref wrap();
     * @note calls @ref ImageView(cth::not_null<Core const*>, Config const&)
     */
    ImageView(cth::not_null<Core const*> core, Config const& config, State const& state);

    /**
     * @note calls @ref optDestroy()
     */
    ~ImageView();


    /**
     * @brief creates the image view
     * @param image requires Image::created()
     * @note calls @ref optDestroy()
     */
    void create(cth::not_null<Image const*> image);

    /**
     * @brief wraps the state with object
     * @note calls @ref optDestroy()
     */
    void wrap(State const& state);

    /**
     * @brief destroys and resets the object
     * @note pushes to @ref Core::destructionQueue() if available
     */
    void destroy();

    /**
     * @brief destroys if @ref created()
     * @note may call @ref destroy()
     */
    void optDestroy() { if(created()) destroy(); }

    /**
     * @brief releases ownership and resets
     * @return objects state
     */
    State release();

    static void destroy(VkDevice vk_device, VkImageView vk_image_view);



    struct Config {
        //VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_NONE; //TODO not supported yet
        uint32_t baseMipLevel = 0;
        uint32_t levelCount = 0; //0 => imageLevels - baseMipLevel
        //uint32_t baseArrayLayer = 0; //TODO not supported yet
        //uint32_t layerCount = 0; //TODO not supported yet

        [[nodiscard]] static Config Default();

    private:
        [[nodiscard]] VkImageSubresourceRange range(uint32_t image_mip_levels, VkImageAspectFlags aspect_mask) const;
        friend class ImageView;
    };

private:
    [[nodiscard]] VkImageViewCreateInfo createViewInfo() const;

    void reset();

    cth::not_null<Core const*> _core;
    Image const* _image = nullptr;
    move_ptr<VkImageView_T> _handle = VK_NULL_HANDLE;

    Config _config;

public:
    [[nodiscard]] bool created() const { return _handle != VK_NULL_HANDLE; }
    [[nodiscard]] VkImageView get() const { return _handle.get(); }
    [[nodiscard]] Image const* image() const { return _image; }

    ImageView(ImageView const& other) = delete;
    ImageView(ImageView&& other) noexcept = default;
    ImageView& operator=(ImageView const& other) = delete;
    ImageView& operator=(ImageView&& other) noexcept = default;

#ifdef CONSTANT_DEBUG_MODE
    static void debug_check(ImageView const* image_view);
    static void debug_check_handle(VkImageView vk_image_view);

#define DEBUG_CHECK_IMAGE_VIEW(image_view) ImageView::debug_check(image_view)
#define DEBUG_CHECK_IMAGE_VIEW_HANDLE(vk_image_view) ImageView::debug_check_handle(vk_image_view)
#else
#define DEBUG_CHECK_IMAGE_VIEW(image_view) ((void)0)
#define DEBUG_CHECK_IMAGE_VIEW_HANDLE(vk_image_view) ((void)0)
#endif

};
} // namespace cth

//State

namespace cth::vk {
struct ImageView::State {
    vk::not_null<VkImageView> vkImageView; // NOLINT(cppcoreguidelines-owning-memory)
    cth::not_null<Image const*> image;
};
}
