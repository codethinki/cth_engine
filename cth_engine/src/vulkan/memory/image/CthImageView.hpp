#pragma once
#include <algorithm>
#include <vulkan/vulkan.h>

namespace cth {
using namespace std;
class Image;

class ImageView {
public:
    struct Config;

    ImageView(Image* image, const Config& config);
    ~ImageView();

private:
    void create(const Config& config);

    Image* _image;
    VkImageView vkImageView = VK_NULL_HANDLE;

public:
    [[nodiscard]] VkImageView get() const { return vkImageView; }
    [[nodiscard]] Image* image() const { return _image; }
};
} // namespace cth

//Config

namespace cth {
struct ImageView::Config {
    explicit Config() = default;
    Config(const VkImageSubresourceRange& range) : baseMipLevel(range.baseMipLevel), levelCount(range.levelCount) {}

    //VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_NONE;
    uint32_t baseMipLevel = 0;
    uint32_t levelCount = 0;
    //uint32_t baseArrayLayer = 0;
    //uint32_t layerCount = 0;

    [[nodiscard]] VkImageSubresourceRange range() const {
        VkImageSubresourceRange range{};
        range.baseMipLevel = baseMipLevel;
        range.levelCount = levelCount;
        return range;
    }
};
}
