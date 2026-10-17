#include "image_layouts.hpp"

#include "cvk/data/img/image.hpp"

#include <cth/io/log.hpp>

#include <algorithm>


namespace cvk {
VkImageLayout ImageLayouts::layout(Image const& image, uint32_t mipLevel) const {
    CTH_CRITICAL(mipLevel >= image.mipLevels(), "mip level must lie in the image") {}

    auto const levels = layouts(image);
    return levels.empty() ? VK_IMAGE_LAYOUT_UNDEFINED : levels[mipLevel];
}

std::span<VkImageLayout const> ImageLayouts::layouts(Image const& image) const {
    auto const it = _layouts.find(image.get());
    if(it == _layouts.end()) return {};

    CTH_CRITICAL(
        it->second.size() != image.mipLevels(),
        "the entry has other mip levels than the image, a destroyed image left it"
    ) {}
    return it->second;
}

void ImageLayouts::set(Image const& image, VkImageLayout layout, uint32_t firstMipLevel, uint32_t mipLevels) {
    Image::debug_check(image);
    CTH_CRITICAL(firstMipLevel >= image.mipLevels(), "first mip level must lie in the image") {}

    uint32_t const count = mipLevels == VK_REMAINING_MIP_LEVELS ? image.mipLevels() - firstMipLevel : mipLevels;
    CTH_CRITICAL(count > image.mipLevels() - firstMipLevel, "mip levels must lie in the image") {}

    auto& levels = _layouts.try_emplace(image.get(), image.mipLevels(), VK_IMAGE_LAYOUT_UNDEFINED).first->second;
    CTH_CRITICAL(
        levels.size() != image.mipLevels(),
        "the entry has other mip levels than the image, a destroyed image left it"
    ) {}

    std::ranges::fill_n(levels.begin() + firstMipLevel, static_cast<ptrdiff_t>(count), layout);
}

void ImageLayouts::erase(Image const& image) { _layouts.erase(image.get()); }

bool ImageLayouts::contains(Image const& image) const { return _layouts.contains(image.get()); }
}
