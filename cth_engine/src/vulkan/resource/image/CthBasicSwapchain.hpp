#pragma once
#include "vulkan/render/pass/cth_render_pass_utils.hpp"
#include "vulkan/resource/image/CthBasicImage.hpp"
#include "vulkan/resource/image/CthImageView.hpp"


#include <vulkan/vulkan.h>


#include <memory>
#include <vector>

namespace cth {
class Queue;
using namespace std;
class BasicCore;
class Device;
class Surface;
class DeletionQueue;
class ImageView;
class Image;
class CmdBuffer;
class PrimaryCmdBuffer;


class BasicSwapchain {
public:
    BasicSwapchain(const BasicCore* core, const Queue* present_queue);
    virtual ~BasicSwapchain();

    //virtual void wrap(const Surface* surface, VkExtent2D window_extent);
    virtual void create(const Surface* surface, VkExtent2D window_extent, const BasicSwapchain* old_swapchain = nullptr);
    virtual void destroy(DeletionQueue* deletion_queue = nullptr);

    virtual void resize(VkExtent2D window_extent);
    virtual void relocate(const Surface* surface, VkExtent2D window_extent);



    [[nodiscard]] VkResult acquireNextImage(uint32_t* image_index) const;

    /**
     * \throws cth::except::vk_result_exception result of vkQueueSubmit()
     */
    VkResult submitCommandBuffer(DeletionQueue* deletion_queue, const PrimaryCmdBuffer* cmd_buffer, uint32_t image_index);

    void changeSwapchainImageQueue(uint32_t release_queue, const CmdBuffer& release_cmd_buffer, uint32_t acquire_queue,
        const CmdBuffer& acquire_cmd_buffer, uint32_t image_index);

    static void destroy(VkDevice device, VkSwapchainKHR swapchain);

private:
    [[nodiscard]] VkResult submit(vector<const PrimaryCmdBuffer*> cmd_buffers) const;
    [[nodiscard]] VkResult present(uint32_t image_index) const;

    //setMsaaSampleCount
    [[nodiscard]] VkSampleCountFlagBits evalMsaaSampleCount() const;

    //createSwapchain
    [[nodiscard]] static VkSurfaceFormatKHR chooseSwapSurfaceFormat(const vector<VkSurfaceFormatKHR>& available_formats);
    [[nodiscard]] static VkPresentModeKHR chooseSwapPresentMode(const vector<VkPresentModeKHR>& available_present_modes);
    [[nodiscard]] static VkExtent2D chooseSwapExtent(VkExtent2D window_extent, const VkSurfaceCapabilitiesKHR& capabilities);
    [[nodiscard]] static uint32_t evalMinImageCount(uint32_t min, uint32_t max);


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


    const BasicCore* _core;
    const Queue* _presentQueue;
    const Surface* _surface;

    VkExtent2D _extent;
    VkExtent2D _windowExtent;


    vector<VkFramebuffer> _swapchainFramebuffers;
    VkRenderPass _renderPass = VK_NULL_HANDLE;

    VkFormat _imageFormat;
    vector<BasicImage> _swapchainImages;
    vector<ImageView> _swapchainImageViews;
    vector<Image> _msaaImages;
    vector<ImageView> _msaaImageViews;

    VkFormat _depthFormat;
    vector<Image> _depthImages;
    vector<ImageView> _depthImageViews;



    VkSwapchainKHR _vkSwapchain;
    shared_ptr<BasicSwapchain> _oldSwapchain; //TODO why is this a shared_ptr?

    vector<VkSemaphore> _imageAvailableSemaphores;
    vector<VkSemaphore> _renderFinishedSemaphores;
    vector<VkFence> _inFlightFences;
    vector<VkFence> _imagesInFlight;

    size_t _currentFrame = 0;

    VkSampleCountFlagBits _msaaSamples = VK_SAMPLE_COUNT_1_BIT;

public:
    [[nodiscard]] auto get() const { return _vkSwapchain; }
    [[nodiscard]] float extentAspectRatio() const { return static_cast<float>(_extent.width) / static_cast<float>(_extent.height); }
    [[nodiscard]] bool compareSwapFormats(const BasicSwapchain& other) const {
        return other._depthFormat != _depthFormat || other._imageFormat != _imageFormat;
    }

    [[nodiscard]] VkFramebuffer framebuffer(const uint32_t index) const { return _swapchainFramebuffers[index]; }
    [[nodiscard]] VkRenderPass renderPass() const { return _renderPass; }
    [[nodiscard]] const ImageView* imageView(const size_t index) const { return &_swapchainImageViews[index]; }
    [[nodiscard]] size_t imageCount() const { return _swapchainImages.size(); }
    [[nodiscard]] VkFormat imageFormat() const { return _imageFormat; }
    [[nodiscard]] VkExtent2D extent() const { return _extent; }
    [[nodiscard]] const BasicImage* image(const size_t index) const { return &_swapchainImages[index]; }
    [[nodiscard]] VkSampleCountFlagBits msaaSamples() const { return _msaaSamples; }


    BasicSwapchain(const BasicSwapchain& other) = default;
    BasicSwapchain(BasicSwapchain&& other) noexcept = default;
    BasicSwapchain& operator=(const BasicSwapchain& other) = default;
    BasicSwapchain& operator=(BasicSwapchain&& other) noexcept = default;

#ifdef CONSTANT_DEBUG_MODE
    static void debug_check(const BasicSwapchain* swapchain);
    static void debug_check_leak(const BasicSwapchain* swapchain);

    static void debug_check_window_extent(VkExtent2D window_extent);
    static void debug_check_compatibility(const BasicSwapchain& a, const BasicSwapchain& b);

#define DEBUG_CHECK_SWAPCHAIN_WINDOW_EXTENT(window_extent) BasicSwapchain::debug_check_window_extent(window_extent);
#define DEBUG_CHECK_SWAPCHAIN_COMPATIBILITY(a_ptr, b_ptr) BasicSwapchain::debug_check_compatibility(a, b)
#define DEBUG_CHECK_SWAPCHAIN_LEAK(swapchain_ptr) debug_check_leak(swapchain_ptr)
#define DEBUG_CHECK_SWAPCHAIN(swapchain_ptr) debug_check(swapchain_ptr)
#define DEBUG_CHECK_SWAPCHAIN_NULLPTR_ALLOWED(swapchain_ptr) if(swapchain_ptr) debug_check(swapchain_ptr)
#else
#define DEBUG_CHECK_SWAPCHAIN_COMPATIBILITY(a_ptr, b_ptr) ((void)0)
#define DEBUG_CHECK_SWAPCHAIN_LEAK(swapchain_ptr) ((void)0)
#define DEBUG_CHECK_SWAPCHAIN_WINDOW_EXTENT(window_extent) ((void)0)
#define DEBUG_CHECK_SWAPCHAIN(swapchain_ptr) ((void)0)
#define DEBUG_CHECK_SWAPCHAIN_NULLPTR_ALLOWED(swapchain_ptr) ((void)0)
#endif

};

}
