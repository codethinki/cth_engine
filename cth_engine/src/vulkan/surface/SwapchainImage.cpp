#include "SwapchainImage.hpp"

#include "CthSwapchain.hpp"
#include "vulkan/memory/image/CthImageView.hpp"

namespace cth {



SwapchainImage::SwapchainImage(Device* device, const Swapchain* swapchain, VkImage image) : BasicImage(swapchain->extent(),
    createImageConfig(swapchain), image) {
    createImageConfig(swapchain);
    createImageView(device);

}
BasicImage::Config SwapchainImage::createImageConfig(const Swapchain* swapchain) {
    BasicImage::Config config;

    config.format = swapchain->imageFormat();
    config.samples = swapchain->msaaSamples();

    config.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    config.mipLevels = 1;
    config.memoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    config.tiling = VK_IMAGE_TILING_OPTIMAL;
    config.usage = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    return config;
}

void SwapchainImage::createImageView(Device* device) { imageView = make_unique<ImageView>(device, this, ImageView::Config::Default()); }
VkImageView SwapchainImage::view() const { return imageView->get(); }


} // namespace cth
