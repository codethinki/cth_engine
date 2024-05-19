#pragma once
#include <cth/cth_memory.hpp>
#include <vulkan/vulkan.h>

#include <algorithm>

namespace cth {
class BasicCore;
class BasicImage;
class Device;

class ImageView {
public:
    struct Config;

    ImageView(const BasicCore* device, BasicImage* image, const Config& config);
    ~ImageView();

private:
    void create(const Config& config);

    const BasicCore* _core;
    BasicImage* _image;
    VkImageView _handle = VK_NULL_HANDLE;

public:
    [[nodiscard]] VkImageView get() const { return _handle; }
    [[nodiscard]] BasicImage* image() const { return _image; }
};
} // namespace cth

//Config

namespace cth {
struct ImageView::Config {
    explicit Config() = default;
    Config(const VkImageSubresourceRange& range) : baseMipLevel(range.baseMipLevel), levelCount(range.levelCount) {}

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
