#pragma once
#include "vulkan/render/pass/cth_render_pass_utils.hpp"

#include <vulkan/vulkan.h>


#include <memory>
#include <vector>

#include "SwapchainImage.hpp"



namespace cth {
using namespace std;

class Device;
class Surface;

class SwapchainImage;

class Swapchain {
public:
    static constexpr VkSampleCountFlagBits MAX_MSAA_SAMPLES = VK_SAMPLE_COUNT_4_BIT;
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

    Swapchain(Device* device, VkExtent2D window_extent, const Surface* surface);
    Swapchain(Device* device, VkExtent2D window_extent, const Surface* surface, shared_ptr<Swapchain> previous);
    ~Swapchain();



    [[nodiscard]] VkResult acquireNextImage(uint32_t* image_index) const;

    /**
     * \throws cth::except::vk_result_exception result of vkQueueSubmit()
     */
    VkResult submitCommandBuffer(VkCommandBuffer buffer, uint32_t image_index);

private:
    //setMsaaSampleCount
    [[nodiscard]] VkSampleCountFlagBits evalMsaaSampleCount() const;

    //createSwapchain
    [[nodiscard]] static VkSurfaceFormatKHR chooseSwapSurfaceFormat(const vector<VkSurfaceFormatKHR>& available_formats);
    [[nodiscard]] static VkPresentModeKHR chooseSwapPresentMode(const vector<VkPresentModeKHR>& available_present_modes);
    [[nodiscard]] VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) const;

    [[nodiscard]] uint32_t evalMinImageCount(uint32_t min, uint32_t max) const;
    void createSwapchain(const Surface* surface);


    void createMsaaResources();
    [[nodiscard]] VkFormat findDepthFormat() const;
    void createDepthResources();
    [[nodiscard]] BasicImage::Config createImageConfig(VkImageAspectFlagBits aspect_mask, VkFormat format, VkImageUsageFlags usage) const;
    void createSwapchainImages();


    //createRenderPass
    [[nodiscard]] VkAttachmentDescription createColorAttachmentDescription() const;
    [[nodiscard]] VkAttachmentDescription createDepthAttachment() const;
    [[nodiscard]] VkAttachmentDescription createColorAttachmentResolve() const;
    [[nodiscard]] SubpassDescription createSubpassDescription() const;
    [[nodiscard]] VkSubpassDependency createSubpassDependency() const;
    /**
     * \throws cth::except::vk_result_exception result of vkCreateRenderPass()
     */
    void createRenderPass();
    /**
     * \throws cth::except::vk_result_exception result of vkCreateFramebuffer()
     */
    void createFramebuffers();
    /**
     * \throws cth::except::vk_result_exception result of vkCreateSemaphore() / vkCreateFence()
     */
    void createSyncObjects();

    void init(const Surface* surface);

    [[nodiscard]] VkResult submit(VkCommandBuffer command_buffer) const;
    [[nodiscard]] VkResult present(uint32_t image_index) const;



    Device* device;
    VkExtent2D _extent;
    VkExtent2D windowExtent;


    vector<VkFramebuffer> swapchainFramebuffers;
    VkRenderPass _renderPass = VK_NULL_HANDLE;

    VkFormat _imageFormat;
    vector<SwapchainImage> swapchainImages;
    vector<Image> msaaImages;
    vector<ImageView> msaaImageViews;

    VkFormat _depthFormat;
    vector<Image> depthImages;
    vector<ImageView> depthImageViews;



    VkSwapchainKHR vkSwapchain;
    shared_ptr<Swapchain> oldSwapchain; //TODO why is this a shared_ptr?

    vector<VkSemaphore> imageAvailableSemaphores;
    vector<VkSemaphore> renderFinishedSemaphores;
    vector<VkFence> inFlightFences;
    vector<VkFence> imagesInFlight;

    size_t currentFrame = 0;

    VkSampleCountFlagBits _msaaSamples = VK_SAMPLE_COUNT_1_BIT;

public:
    [[nodiscard]] float extentAspectRatio() const { return static_cast<float>(_extent.width) / static_cast<float>(_extent.height); }
    [[nodiscard]] bool compareSwapFormats(const Swapchain& other) const {
        return other._depthFormat != _depthFormat || other._imageFormat != _imageFormat;
    }

    [[nodiscard]] VkFramebuffer framebuffer(const uint32_t index) const { return swapchainFramebuffers[index]; }
    [[nodiscard]] VkRenderPass renderPass() const { return _renderPass; }
    [[nodiscard]] VkImageView imageView(const size_t index) const { return swapchainImages[index].view(); }
    [[nodiscard]] size_t imageCount() const { return swapchainImages.size(); }
    [[nodiscard]] VkFormat imageFormat() const { return _imageFormat; }
    [[nodiscard]] VkExtent2D extent() const { return _extent; }
    [[nodiscard]] const SwapchainImage* image(const size_t index) const { return &swapchainImages[index]; }
    [[nodiscard]] VkSampleCountFlagBits msaaSamples() const { return _msaaSamples; }


    Swapchain(const Swapchain&) = delete;
    Swapchain& operator=(const Swapchain&) = delete;

};

}
