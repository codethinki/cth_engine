#pragma once
#include "../graphics_core/CthGraphicsSyncConfig.hpp"
#include "vulkan/base/CthQueue.hpp"
#include "vulkan/render/control/CthFence.hpp"
#include "vulkan/render/pass/cth_render_pass_utils.hpp"
#include "vulkan/resource/image/CthBasicImage.hpp"
#include "vulkan/resource/image/CthImageView.hpp"

#include <vulkan/vulkan.h>


#include <memory>
#include <vector>



namespace cth::vk {
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
    BasicSwapchain(BasicCore const* core, Queue const* present_queue, BasicGraphicsSyncConfig const* sync_config);
    virtual ~BasicSwapchain();

    //IMPLEMENT virtual void wrap(const Surface* surface, VkExtent2D window_extent);
    virtual void create(Surface const* surface, VkExtent2D window_extent, BasicSwapchain const* old_swapchain = nullptr);
    /**
     * @brief destroys the swapchain
     * @note implicitly calls destroyResources()
     */
    virtual void destroy(DeletionQueue* deletion_queue = nullptr);


    virtual void resize(VkExtent2D window_extent);
    virtual void relocate(Surface const* surface, VkExtent2D window_extent);



    /**
     * @note might block
     * @return result of vkAcquireNextImageKHR() [VK_SUCCESS, VK_SUBOPTIMAL_KHR]
     *
     */
    [[nodiscard]] VkResult acquireNextImage();

    void beginRenderPass(PrimaryCmdBuffer const* cmd_buffer) const;

    void endRenderPass(PrimaryCmdBuffer const* cmd_buffer);


    [[nodiscard]] VkResult present(DeletionQueue* deletion_queue); //TEMP remove deletion queue from here


    void changeSwapchainImageQueue(uint32_t release_queue, CmdBuffer const& release_cmd_buffer, uint32_t acquire_queue,
        CmdBuffer const& acquire_cmd_buffer, uint32_t image_index);

    static void destroy(VkDevice device, VkSwapchainKHR swapchain);

private:
    static constexpr uint32_t NO_IMAGE_INDEX = std::numeric_limits<uint32_t>::max();

    VkResult acquireNewImage(size_t frame);

    //setMsaaSampleCount
    [[nodiscard]] VkSampleCountFlagBits evalMsaaSampleCount() const;


    //createSyncObjects
    void createSyncObjects();

    //createSwapchain
    [[nodiscard]] static VkSurfaceFormatKHR chooseSwapSurfaceFormat(std::vector<VkSurfaceFormatKHR> const& available_formats);
    [[nodiscard]] static VkPresentModeKHR chooseSwapPresentMode(std::vector<VkPresentModeKHR> const& available_present_modes);
    [[nodiscard]] static VkExtent2D chooseSwapExtent(VkExtent2D window_extent, VkSurfaceCapabilitiesKHR const& capabilities);
    [[nodiscard]] static uint32_t evalMinImageCount(uint32_t min, uint32_t max);
    [[nodiscard]] static VkSwapchainCreateInfoKHR createInfo(Surface const* surface, VkSurfaceFormatKHR surface_format,
        VkSurfaceCapabilitiesKHR const& capabilities, VkPresentModeKHR present_mode, VkExtent2D extent, uint32_t image_count,
        BasicSwapchain const* old_swapchain);
    void createSwapchain(Surface const* surface, VkExtent2D window_extent, BasicSwapchain const* old_swapchain);

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



    BasicCore const* _core;
    Queue const* _presentQueue;
    Surface const* _surface = nullptr;

    cth::move_ptr<VkSwapchainKHR_T> _handle = VK_NULL_HANDLE;
    std::shared_ptr<BasicSwapchain> _oldSwapchain; //TODO why is this a shared_ptr?


    VkExtent2D _extent{};
    float _aspectRatio = 0;
    VkExtent2D _windowExtent{};


    std::vector<VkFramebuffer> _swapchainFramebuffers;
    [[nodiscard]] VkFramebuffer framebuffer() const { return _swapchainFramebuffers[imageIndex()]; }

    VkRenderPass _renderPass = VK_NULL_HANDLE;

    VkFormat _imageFormat{};
    std::vector<BasicImage> _swapchainImages;
    std::vector<ImageView> _swapchainImageViews;
    std::vector<Image> _msaaImages;
    std::vector<ImageView> _msaaImageViews;

    VkFormat _depthFormat{};
    std::vector<Image> _depthImages;
    std::vector<ImageView> _depthImageViews;

    BasicGraphicsSyncConfig const* _syncConfig;

    std::vector<Fence> _imageAvailableFences;

    std::vector<Queue::PresentInfo> _presentInfos;

    size_t _frameIndex = 0;
    static size_t nextFrame(size_t const current) { return (current + 1) % constants::FRAMES_IN_FLIGHT; }

    std::array<uint32_t, constants::FRAMES_IN_FLIGHT> _imageIndices{};
    [[nodiscard]] uint32_t imageIndex() const { return _imageIndices[_frameIndex]; }

    VkSampleCountFlagBits _msaaSamples = VK_SAMPLE_COUNT_1_BIT;


public:
    [[nodiscard]] VkSwapchainKHR get() const { return _handle.get(); }
    [[nodiscard]] float extentAspectRatio() const { return _aspectRatio; }
    [[nodiscard]] bool compareSwapFormats(BasicSwapchain const& other) const {
        return other._depthFormat != _depthFormat || other._imageFormat != _imageFormat;
    }

    [[nodiscard]] ImageView const* imageView(size_t const index) const { return &_swapchainImageViews[index]; }
    [[nodiscard]] size_t imageCount() const { return _swapchainImages.size(); }
    [[nodiscard]] VkFormat imageFormat() const { return _imageFormat; }
    [[nodiscard]] BasicImage const* image(size_t const index) const { return &_swapchainImages[index]; }
    [[nodiscard]] VkSampleCountFlagBits msaaSamples() const { return _msaaSamples; } //TODO move this to framebuffer or render pass
    [[nodiscard]] VkRenderPass renderPass() const { return _renderPass; }



    BasicSwapchain(BasicSwapchain const& other) = default;
    BasicSwapchain(BasicSwapchain&& other) noexcept = default;
    BasicSwapchain& operator=(BasicSwapchain const& other) = default;
    BasicSwapchain& operator=(BasicSwapchain&& other) noexcept = default;

#ifdef CONSTANT_DEBUG_MODE
    static void debug_check(BasicSwapchain const* swapchain);
    static void debug_check_leak(BasicSwapchain const* swapchain);

    static void debug_check_window_extent(VkExtent2D window_extent);
    static void debug_check_compatibility(BasicSwapchain const& a, BasicSwapchain const& b);

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
