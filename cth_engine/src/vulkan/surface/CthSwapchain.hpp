#pragma once
#include "interface/CthEngineSettings.hpp"
#include "vulkan/render/pass/cth_render_pass_utils.hpp"
#include "vulkan/resource/image/CthBasicImage.hpp"
#include "vulkan/resource/image/CthImageView.hpp"


#include <vulkan/vulkan.h>


#include <memory>
#include <vector>

namespace cth {
using namespace std;

class Device;
class Surface;
class DeletionQueue;
class ImageView;
class Image;
class CmdBuffer;
class PrimaryCmdBuffer;


class Swapchain {
public:
    Swapchain(Device* device, DeletionQueue* deletion_queue, VkExtent2D window_extent, const Surface* surface);
    Swapchain(Device* device, DeletionQueue* deletion_queue, VkExtent2D window_extent, const Surface* surface, shared_ptr<Swapchain> previous);
    ~Swapchain();



    [[nodiscard]] VkResult acquireNextImage(uint32_t* image_index) const;

    /**
     * \throws cth::except::vk_result_exception result of vkQueueSubmit()
     */
    VkResult submitCommandBuffer(DeletionQueue* deletion_queue, const PrimaryCmdBuffer* cmd_buffer, uint32_t image_index);

    void changeSwapchainImageQueue(uint32_t release_queue, const CmdBuffer* release_cmd_buffer, uint32_t acquire_queue,
        const CmdBuffer* acquire_cmd_buffer, uint32_t image_index);

private:
    [[nodiscard]] VkResult submit(vector<const PrimaryCmdBuffer*> cmd_buffers) const;
    [[nodiscard]] VkResult present(uint32_t image_index) const;

    //setMsaaSampleCount
    [[nodiscard]] VkSampleCountFlagBits evalMsaaSampleCount() const;

    //createSwapchain
    [[nodiscard]] static VkSurfaceFormatKHR chooseSwapSurfaceFormat(const vector<VkSurfaceFormatKHR>& available_formats);
    [[nodiscard]] static VkPresentModeKHR chooseSwapPresentMode(const vector<VkPresentModeKHR>& available_present_modes);
    [[nodiscard]] VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) const;
    [[nodiscard]] uint32_t evalMinImageCount(uint32_t min, uint32_t max) const;

    void createSwapchain(const Surface* surface);

    [[nodiscard]] BasicImage::Config createImageConfig() const;
    [[nodiscard]] BasicImage::Config createColorImageConfig() const;
    void createSwapchainImages();
    void createMsaaResources(DeletionQueue* deletion_queue); //TEMP remove deletion queue argument
    [[nodiscard]] BasicImage::Config createDepthImageConfig() const;
    [[nodiscard]] VkFormat findDepthFormat() const;
    void createDepthResources(DeletionQueue* deletion_queue);



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



    void init(const Surface* surface, DeletionQueue* deletion_queue);


    Device* _device;
    VkExtent2D _extent;
    VkExtent2D _windowExtent;


    vector<VkFramebuffer> _swapchainFramebuffers;
    VkRenderPass _renderPass = VK_NULL_HANDLE;

    VkFormat _imageFormat;
    vector<unique_ptr<BasicImage>> _swapchainImages;
    vector<ImageView> _swapchainImageViews;
    vector<unique_ptr<Image>> _msaaImages;
    vector<ImageView> _msaaImageViews;

    VkFormat _depthFormat;
    vector<unique_ptr<Image>> _depthImages;
    vector<ImageView> _depthImageViews;



    VkSwapchainKHR _vkSwapchain;
    shared_ptr<Swapchain> _oldSwapchain; //TODO why is this a shared_ptr?

    vector<VkSemaphore> _imageAvailableSemaphores;
    vector<VkSemaphore> _renderFinishedSemaphores;
    vector<VkFence> _inFlightFences;
    vector<VkFence> _imagesInFlight;

    size_t _currentFrame = 0;

    VkSampleCountFlagBits _msaaSamples = VK_SAMPLE_COUNT_1_BIT;

public:
    [[nodiscard]] auto get() const { return _vkSwapchain; }
    [[nodiscard]] float extentAspectRatio() const { return static_cast<float>(_extent.width) / static_cast<float>(_extent.height); }
    [[nodiscard]] bool compareSwapFormats(const Swapchain& other) const {
        return other._depthFormat != _depthFormat || other._imageFormat != _imageFormat;
    }

    [[nodiscard]] VkFramebuffer framebuffer(const uint32_t index) const { return _swapchainFramebuffers[index]; }
    [[nodiscard]] VkRenderPass renderPass() const { return _renderPass; }
    [[nodiscard]] const ImageView* imageView(const size_t index) const { return &_swapchainImageViews[index]; }
    [[nodiscard]] size_t imageCount() const { return _swapchainImages.size(); }
    [[nodiscard]] VkFormat imageFormat() const { return _imageFormat; }
    [[nodiscard]] VkExtent2D extent() const { return _extent; }
    [[nodiscard]] const BasicImage* image(const size_t index) const { return _swapchainImages[index].get(); }
    [[nodiscard]] VkSampleCountFlagBits msaaSamples() const { return _msaaSamples; }


    Swapchain(const Swapchain&) = delete;
    Swapchain& operator=(const Swapchain&) = delete;

};

}
