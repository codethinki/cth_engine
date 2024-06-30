#pragma once
#include "vulkan/base/CthQueue.hpp"
#include "vulkan/render/control/CthFence.hpp"
#include "vulkan/render/pass/cth_render_pass_utils.hpp"
#include "vulkan/resource/image/CthBasicImage.hpp"
#include "vulkan/resource/image/CthImageView.hpp"


#include <vulkan/vulkan.h>


#include <memory>
#include <vector>

#include "../graphics_core/CthGraphicsSyncConfig.hpp"


namespace cth {
class Queue;
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
    BasicSwapchain(const BasicCore* core, const Queue* present_queue, BasicGraphicsSyncConfig sync_config);
    virtual ~BasicSwapchain();

    //virtual void wrap(const Surface* surface, VkExtent2D window_extent);
    virtual void create(const Surface* surface, VkExtent2D window_extent, const BasicSwapchain* old_swapchain = nullptr);
    /**
     * @brief destroys the swapchain
     * @note implicitly calls destroyResources()
     */
    virtual void destroy(DeletionQueue* deletion_queue = nullptr);


    virtual void resize(VkExtent2D window_extent);
    virtual void relocate(const Surface* surface, VkExtent2D window_extent);



    /**
     * @note might block
     * @return result of vkAcquireNextImageKHR() [VK_SUCCESS, VK_SUBOPTIMAL_KHR]
     *
     */
    [[nodiscard]] VkResult acquireNextImage();

    void beginRenderPass(const PrimaryCmdBuffer* cmd_buffer) const;

    void endRenderPass(const PrimaryCmdBuffer* cmd_buffer);


    [[nodiscard]] VkResult present(DeletionQueue* deletion_queue); //TEMP remove deletion queue from here


    void changeSwapchainImageQueue(uint32_t release_queue, const CmdBuffer& release_cmd_buffer, uint32_t acquire_queue,
        const CmdBuffer& acquire_cmd_buffer, uint32_t image_index);

    static void destroy(VkDevice device, VkSwapchainKHR swapchain);

private:
    VkResult acquireNewImage(const size_t frame);

    //setMsaaSampleCount
    [[nodiscard]] VkSampleCountFlagBits evalMsaaSampleCount() const;


    //createSwapchain
    [[nodiscard]] static VkSurfaceFormatKHR chooseSwapSurfaceFormat(const vector<VkSurfaceFormatKHR>& available_formats);
    [[nodiscard]] static VkPresentModeKHR chooseSwapPresentMode(const vector<VkPresentModeKHR>& available_present_modes);
    [[nodiscard]] static VkExtent2D chooseSwapExtent(VkExtent2D window_extent, const VkSurfaceCapabilitiesKHR& capabilities);
    [[nodiscard]] static uint32_t evalMinImageCount(uint32_t min, uint32_t max);
    [[nodiscard]] static VkSwapchainCreateInfoKHR createInfo(const Surface* surface, VkSurfaceFormatKHR surface_format,
        const VkSurfaceCapabilitiesKHR& capabilities, VkPresentModeKHR present_mode, VkExtent2D extent, uint32_t image_count,
        const BasicSwapchain* old_swapchain);
    void createSwapchain(const Surface* surface, VkExtent2D window_extent, const BasicSwapchain* old_swapchain);

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
     * @throws cth::except::vk_result_exception result of vkCreateRenderPass()
     */
    void createRenderPass();
    /**
     * @throws cth::except::vk_result_exception result of vkCreateFramebuffer()
     */
    void createFramebuffers();

    void createPresentInfos();


    void clearPresentInfos();
    void destroyFramebuffers(DeletionQueue* deletion_queue);
    void destroyRenderPass(DeletionQueue* deletion_queue);

    void destroyDepthImages(DeletionQueue* deletion_queue);
    void destroyMsaaImages(DeletionQueue* deletion_queue);
    void destroySwapchainImages(DeletionQueue* deletion_queue);
    void destroyImages(DeletionQueue* deletion_queue);
    /**
    * @brief destroys everything that is not the swapchain
    */
    virtual void destroyResources(DeletionQueue* deletion_queue = nullptr);

    void destroySwapchain(DeletionQueue* deletion_queue);

    void destroySyncObjects(DeletionQueue* deletion_queue);



    const BasicCore* _core;
    const Queue* _presentQueue;
    const Surface* _surface;

    cth::ptr::mover<VkSwapchainKHR_T> _handle = VK_NULL_HANDLE;
    shared_ptr<BasicSwapchain> _oldSwapchain; //TODO why is this a shared_ptr?


    VkExtent2D _extent;
    float _aspectRatio;
    VkExtent2D _windowExtent;


    vector<VkFramebuffer> _swapchainFramebuffers;
    [[nodiscard]] VkFramebuffer framebuffer() const { return _swapchainFramebuffers[imageIndex()]; }

    VkRenderPass _renderPass = VK_NULL_HANDLE;

    VkFormat _imageFormat;
    vector<BasicImage> _swapchainImages;
    vector<ImageView> _swapchainImageViews;
    vector<Image> _msaaImages;
    vector<ImageView> _msaaImageViews;

    VkFormat _depthFormat;
    vector<Image> _depthImages;
    vector<ImageView> _depthImageViews;

    BasicGraphicsSyncConfig _syncConfig;

    std::vector<Fence> _imageAvailableFences;

    vector<Queue::PresentInfo> _presentInfos;

    size_t _currentFrame = 0;
    static size_t nextFrame(const size_t current) { return (current + 1) % Constant::FRAMES_IN_FLIGHT; }

    vector<uint32_t> _imageIndices;
    [[nodiscard]] uint32_t imageIndex() const { return _imageIndices[_currentFrame]; }

    VkSampleCountFlagBits _msaaSamples = VK_SAMPLE_COUNT_1_BIT;

    static constexpr uint32_t NO_IMAGE_INDEX = UINT32_MAX;

public:
    [[nodiscard]] VkSwapchainKHR get() const { return _handle.get(); }
    [[nodiscard]] float extentAspectRatio() const { return _aspectRatio; }
    [[nodiscard]] bool compareSwapFormats(const BasicSwapchain& other) const {
        return other._depthFormat != _depthFormat || other._imageFormat != _imageFormat;
    }

    [[nodiscard]] const ImageView* imageView(const size_t index) const { return &_swapchainImageViews[index]; }
    [[nodiscard]] size_t imageCount() const { return _swapchainImages.size(); }
    [[nodiscard]] VkFormat imageFormat() const { return _imageFormat; }
    [[nodiscard]] const BasicImage* image(const size_t index) const { return &_swapchainImages[index]; }
    [[nodiscard]] VkSampleCountFlagBits msaaSamples() const { return _msaaSamples; } //TODO move this to framebuffer or render pass
    [[nodiscard]] VkRenderPass renderPass() const { return _renderPass; }



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
#define DEBUG_CHECK_SWAPCHAIN_LEAK(swapchain_ptr) BasicSwapchain::debug_check_leak(swapchain_ptr)
#define DEBUG_CHECK_SWAPCHAIN(swapchain_ptr) BasicSwapchain::debug_check(swapchain_ptr)
#define DEBUG_CHECK_SWAPCHAIN_NULLPTR_ALLOWED(swapchain_ptr) if(swapchain_ptr) BasicSwapchain::debug_check(swapchain_ptr)
#else
#define DEBUG_CHECK_SWAPCHAIN_COMPATIBILITY(a_ptr, b_ptr) ((void)0)
#define DEBUG_CHECK_SWAPCHAIN_LEAK(swapchain_ptr) ((void)0)
#define DEBUG_CHECK_SWAPCHAIN_WINDOW_EXTENT(window_extent) ((void)0)
#define DEBUG_CHECK_SWAPCHAIN(swapchain_ptr) ((void)0)
#define DEBUG_CHECK_SWAPCHAIN_NULLPTR_ALLOWED(swapchain_ptr) ((void)0)
#endif

};

}



//TEMP old code
//
//
///**
// * @return result of vkQueueSubmit() [VK_SUCCESS, VK_SUBOPTIMAL_KHR]
// * @note implicitly calls presentQueue->present()
// */
//VkResult submitCommandBuffer(DeletionQueue* deletion_queue, const PrimaryCmdBuffer* cmd_buffer, uint32_t image_index);
