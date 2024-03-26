#pragma once
#include "vulkan/render/pass/cth_render_pass_utils.hpp"

#include <vulkan/vulkan.h>


#include <memory>
#include <vector>



namespace cth {
class Window;
using namespace std;

class Device;

class Swapchain {
public:
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

    //TODO maybe move this to the image class
    static VkImageView createImageView(const VkDevice& device, VkImage image, VkFormat format,
        VkImageAspectFlags aspect_flags = VK_IMAGE_ASPECT_COLOR_BIT, uint32_t mip_levels = 1);

    [[nodiscard]] VkFormat findDepthFormat() const;

    [[nodiscard]] VkResult acquireNextImage(uint32_t* image_index) const;

    /**
     * \throws cth::except::vk_result_exception result of vkQueueSubmit()
     */
    VkResult submitCommandBuffer(VkCommandBuffer buffer, uint32_t image_index);


private:
    //createSwapchain
    [[nodiscard]] static VkSurfaceFormatKHR chooseSwapSurfaceFormat(const vector<VkSurfaceFormatKHR>& available_formats);
    [[nodiscard]] static VkPresentModeKHR chooseSwapPresentMode(const vector<VkPresentModeKHR>& available_present_modes);
    [[nodiscard]] VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) const;
    void createSwapchain();
    //setImageCount
    void setImageCount();
    //createImageViews
    void createImageViews();
    //setMsaaSampleCount
    [[nodiscard]] VkSampleCountFlagBits evaluateMsaaSampleCount() const;
    void setMsaaSampleCount();

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
    //createColorImageInfo
    [[nodiscard]] VkImageCreateInfo createColorImageInfo() const;
    void createColorResources();
    //createDepthResources
    [[nodiscard]] VkImageCreateInfo createDepthImageInfo(VkFormat depth_format) const;
    void createDepthResources();

    /**
     * \throws cth::except::vk_result_exception result of vkCreateFramebuffer()
     */
    void createFramebuffers();
    /**
     * \throws cth::except::vk_result_exception result of vkCreateSemaphore() / vkCreateFence()
     */
    void createSyncObjects();

    void init();

    //submitCommandBuffer helpers
    [[nodiscard]] VkResult submit(VkCommandBuffer command_buffer) const;
    [[nodiscard]] VkResult present(uint32_t image_index) const;



    VkFormat swapchainImageFormat;
    VkFormat swapchainDepthFormat;
    VkExtent2D swapchainExtent;

    vector<VkFramebuffer> swapchainFramebuffers;
    VkRenderPass renderPass;

    vector<VkImage> swapchainImages;
    vector<VkImageView> swapchainImageViews;

    vector<VkImage> depthImages;
    vector<VkImageView> depthImageViews;
    vector<VkDeviceMemory> depthImageMemories;

    vector<VkImage> msaaImages;
    vector<VkImageView> msaaImageViews;
    vector<VkDeviceMemory> msaaImageMemories;


    Device* device;
    Window* window;
    VkExtent2D windowExtent;

    VkSwapchainKHR vkSwapchain;
    shared_ptr<Swapchain> oldSwapchain; //TODO why is this a shared_ptr?

    vector<VkSemaphore> imageAvailableSemaphores;
    vector<VkSemaphore> renderFinishedSemaphores;
    vector<VkFence> inFlightFences;
    vector<VkFence> imagesInFlight;
    size_t currentFrame = 0;

    VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT;
    static constexpr VkSampleCountFlagBits MAX_MSAA_SAMPLES = VK_SAMPLE_COUNT_4_BIT;

public:
    Swapchain(Device* device, Window* window, VkExtent2D window_extent);
    Swapchain(Device* device, Window* window, VkExtent2D window_extent, shared_ptr<Swapchain> previous);
    ~Swapchain();

    [[nodiscard]] float extentAspectRatio() const { return static_cast<float>(swapchainExtent.width) / static_cast<float>(swapchainExtent.height); }
    [[nodiscard]] bool compareSwapFormats(const Swapchain& other) const {
        return other.swapchainDepthFormat == swapchainDepthFormat && other.swapchainImageFormat ==
            swapchainImageFormat;
    }

    [[nodiscard]] VkFramebuffer getFrameBuffer(const uint32_t index) const { return swapchainFramebuffers[index]; }
    [[nodiscard]] VkRenderPass getRenderPass() const { return renderPass; }
    [[nodiscard]] VkImageView getImageView(const uint32_t index) const { return swapchainImageViews[index]; }
    [[nodiscard]] size_t imageCount() const { return swapchainImages.size(); }
    [[nodiscard]] VkFormat getSwapchainImageFormat() const { return swapchainImageFormat; }
    [[nodiscard]] VkExtent2D getSwapchainExtent() const { return swapchainExtent; }
    [[nodiscard]] uint32_t width() const { return swapchainExtent.width; }
    [[nodiscard]] uint32_t height() const { return swapchainExtent.height; }
    [[nodiscard]] VkImage getSwapchainImage(const int index) const { return swapchainImages[index]; }
    [[nodiscard]] VkSampleCountFlagBits getMsaaSampleCount() const { return msaaSamples; }


    Swapchain(const Swapchain&) = delete;
    Swapchain& operator=(const Swapchain&) = delete;

};

}
