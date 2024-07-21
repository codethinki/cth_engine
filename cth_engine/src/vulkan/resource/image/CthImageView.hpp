#pragma once
#include <vulkan/vulkan.h>

namespace cth::vk {
class BasicCore;
class BasicImage;
class Device;

class ImageView {
public:
    struct Config;

    ImageView(BasicCore const* device, BasicImage const* image, Config const& config);
    ~ImageView();

private:
    void create(Config const& config);

    BasicCore const* _core;
    BasicImage const* _image;
    move_ptr<VkImageView_T> _handle = VK_NULL_HANDLE;

public:
    [[nodiscard]] VkImageView get() const { return _handle.get(); }
    [[nodiscard]] BasicImage const* image() const { return _image; }

    ImageView(ImageView const& other) = delete;
    ImageView(ImageView&& other) noexcept = default;
    ImageView& operator=(ImageView const& other) = delete;
    ImageView& operator=(ImageView&& other) noexcept = default;
};
} // namespace cth

//Config

namespace cth::vk {
struct ImageView::Config {
    explicit Config() = default;

    //VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_NONE;
    uint32_t baseMipLevel = 0;
    uint32_t levelCount = 0; //0 => imageLevels - baseMipLevel
    //uint32_t baseArrayLayer = 0;
    //uint32_t layerCount = 0;

    [[nodiscard]] static Config Default();

    [[nodiscard]] VkImageSubresourceRange range() const {
        VkImageSubresourceRange range{};
        range.baseMipLevel = baseMipLevel;
        range.levelCount = levelCount;
        range.baseArrayLayer = 0;
        range.layerCount = 1;
        return range;
    }
};

inline ImageView::Config ImageView::Config::Default() {
    Config config;
    config.baseMipLevel = 0;
    config.levelCount = 1;
    return config;
}

}
