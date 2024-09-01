#pragma once

#include "interface/render/CthRenderCycle.hpp"

#include "vulkan/base/CthQueue.hpp"
#include "vulkan/resource/image/CthImage.hpp"


#include <vulkan/vulkan.h>


#include <memory>
#include <vector>



namespace cth::vk {
class Fence;
class Subpass;
class RenderPass;
class AttachmentCollection;
class Image;
class Framebuffer;
class ImageView;
class CmdBuffer;
class PrimaryCmdBuffer;
class Surface;
struct BasicGraphicsSyncConfig;
class Queue;
class BasicCore;

//TEMP make this non basic and remove components like renderpass and subpass as well as attachments (except resolve attachment)
//TEMP maybe remove framebuffers idk

class BasicSwapchain {
public:
    BasicSwapchain(not_null<BasicCore const*> core, not_null<Queue const*> present_queue, not_null<BasicGraphicsSyncConfig const*> sync_config,
        not_null<Surface const*> surface);
    virtual ~BasicSwapchain();

    //IMPLEMENT virtual void wrap(const Surface* surface, VkExtent2D window_extent);
    virtual void create(VkExtent2D window_extent, VkSwapchainKHR old_swapchain = VK_NULL_HANDLE);
    /**
     * @brief destroys the swapchain
     * @note calls destroyResources()
     */
    virtual void destroy();


    virtual void resize(VkExtent2D window_extent);



    /**
     * @return result of vkAcquireNextImageKHR() [VK_SUCCESS, VK_SUBOPTIMAL_KHR]
     *
     * @note might block
     * @note the semaphore must not be signaled
     * @note the fence must be signaled
     */
    VkResult acquireNextImage(Cycle const& cycle);
    void skipAcquire(Cycle const& cycle);

    void beginRenderPass(Cycle const& cycle, PrimaryCmdBuffer const* cmd_buffer) const;

    void endRenderPass(PrimaryCmdBuffer const* cmd_buffer);


    [[nodiscard]] VkResult present(Cycle const& cycle); //TEMP remove deletion queue from here
    void skipPresent(Cycle const& cycle);

    void changeSwapchainImageQueue(uint32_t release_queue, CmdBuffer const& release_cmd_buffer, uint32_t acquire_queue,
        CmdBuffer const& acquire_cmd_buffer, uint32_t image_index);

    [[nodiscard]] ImageView const* imageView(size_t index) const;
    [[nodiscard]] Image const* image(size_t index) const;


    static void destroy(VkDevice device, VkSwapchainKHR swapchain);

private:
    static constexpr uint32_t NO_IMAGE_INDEX = std::numeric_limits<uint32_t>::max();


    //setMsaaSampleCount
    [[nodiscard]] VkSampleCountFlagBits evalMsaaSampleCount() const;


    //createSyncObjects
    void createSyncObjects();

    //createSwapchain
    [[nodiscard]] static VkSurfaceFormatKHR chooseSwapSurfaceFormat(std::vector<VkSurfaceFormatKHR> const& available_formats);
    [[nodiscard]] static VkPresentModeKHR chooseSwapPresentMode(std::vector<VkPresentModeKHR> const& available_present_modes);
    [[nodiscard]] static VkExtent2D chooseSwapExtent(VkExtent2D window_extent, VkSurfaceCapabilitiesKHR const& capabilities);
    [[nodiscard]] static uint32_t evalMinImageCount(uint32_t min, uint32_t max);
    [[nodiscard]] static VkSwapchainCreateInfoKHR createInfo(VkSurfaceKHR surface,
        VkSurfaceFormatKHR surface_format, VkSurfaceCapabilitiesKHR const& capabilities, VkPresentModeKHR present_mode, VkExtent2D extent,
        uint32_t image_count, VkSwapchainKHR old_swapchain);
    void createSwapchain(VkExtent2D window_extent, VkSwapchainKHR old_swapchain);


    [[nodiscard]] Image::Config createColorImageConfig(VkSampleCountFlagBits samples) const;
    [[nodiscard]] Image::Config createDepthImageConfig() const;

    [[nodiscard]] std::vector<VkImage> getSwapchainImages();
    void findDepthFormat();


    void createResolveAttachments(std::vector<VkImage> swapchain_images);
    void createMsaaAttachments();
    void createDepthAttachments();

    void createAttachments();


    //createRenderPass
    void createSubpass();
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


    void destroyRenderConstructs();
    void destroyResources();



    void destroySwapchain();

    void destroySyncObjects(DestructionQueue* destruction_queue);
    //TEMP left off here check swapchain destruction and then try to make it compile

    void resizeReset();
    void reset();

    not_null<BasicCore const*> _core;
    not_null<Queue const*> _presentQueue;
    not_null<Surface const*> _surface;


    cth::move_ptr<VkSwapchainKHR_T> _handle = VK_NULL_HANDLE;
    std::shared_ptr<BasicSwapchain> _oldSwapchain; //TODO why is this a shared_ptr?


    VkExtent2D _extent{};
    float _aspectRatio = 0;
    VkExtent2D _windowExtent{};


    size_t _imageCount = 0;
    VkFormat _imageFormat{};
    VkFormat _depthFormat{};

    std::unique_ptr<AttachmentCollection> _resolveAttachments;
    std::unique_ptr<AttachmentCollection> _msaaAttachments; //TEMP this should not be here
    std::unique_ptr<AttachmentCollection> _depthAttachments; //TEMP this should not be here

    std::unique_ptr<RenderPass> _renderPass; //TEMP this should not be here
    std::unique_ptr<Subpass> _subpass; //TEMP this should not be here
    std::vector<Framebuffer> _swapchainFramebuffers; //TEMP this maybe should not be here


    not_null<BasicGraphicsSyncConfig const*> _syncConfig;

    std::vector<Fence> _imageAvailableFences;

    std::vector<Queue::PresentInfo> _presentInfos;


    std::array<uint32_t, constants::FRAMES_IN_FLIGHT> _imageIndices{};

    VkSampleCountFlagBits _msaaSamples = VK_SAMPLE_COUNT_1_BIT;

public:
    [[nodiscard]] VkSwapchainKHR get() const { return _handle.get(); }
    [[nodiscard]] float extentAspectRatio() const { return _aspectRatio; }
    [[nodiscard]] bool compareSwapFormats(BasicSwapchain const& other) const {
        return other._depthFormat != _depthFormat || other._imageFormat != _imageFormat;
    }
    [[nodiscard]] RenderPass const* renderPass() const { return _renderPass.get(); }

    [[nodiscard]] size_t imageCount() const { return _imageCount; }
    [[nodiscard]] VkFormat imageFormat() const { return _imageFormat; }
    [[nodiscard]] VkSampleCountFlagBits msaaSamples() const { return _msaaSamples; } //TODO move this to framebuffer or render pass


    BasicSwapchain(BasicSwapchain const& other) = delete;
    BasicSwapchain(BasicSwapchain&& other) noexcept = default;
    BasicSwapchain& operator=(BasicSwapchain const& other) = delete;
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
// * @note calls presentQueue->present()
// */
//VkResult submitCommandBuffer(DestructionQueue* destruction_queue, const PrimaryCmdBuffer* cmd_buffer, uint32_t image_index);
