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
    Config(const VkImageSubresourceRange& range) : range{range} {}

    VkImageSubresourceRange range{VK_IMAGE_ASPECT_NONE, 0, 0, 0, 0};
};
}
