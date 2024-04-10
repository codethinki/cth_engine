#include "CthBasicImage.hpp"

#include <iterator>


namespace cth {

BasicImage::BasicImage(const VkExtent2D extent, const Config& config, VkImage image) : _extent(extent), _config(config), vkImage(image) {
    std::ranges::fill_n(std::back_inserter(imageLayouts), _config.mipLevels, VK_IMAGE_LAYOUT_UNDEFINED);
}

}

//Config

namespace cth {
VkImageCreateInfo BasicImage::Config::createInfo() const {
    VkImageCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;

    info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.imageType = VK_IMAGE_TYPE_2D;
    info.arrayLayers = 1;
    info.extent.depth = 1;

    info.format = format;
    info.tiling = tiling;
    info.usage = usage;
    info.mipLevels = mipLevels;
    info.samples = samples;

    return info;
}
} // namespace cth
