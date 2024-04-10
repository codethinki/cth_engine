#pragma once
#include "vulkan/memory/image/CthImage.hpp"

#include <memory>

namespace cth {
using namespace cth;
class Device;
class ImageView;
class Swapchain;

class SwapchainImage : public BasicImage {
public:
    SwapchainImage(Device* device, const Swapchain* swapchain, VkImage image);

private:
    static BasicImage::Config createImageConfig(const Swapchain* swapchain);
    void createImageView(Device* device);

    unique_ptr<ImageView> imageView;
public:
    [[nodiscard]] VkImageView view() const;
};
} // namespace cth
